#include "search.h"
#include "xxHash/xxhash.h"
#include "json/single_include/nlohmann/json.hpp"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <stack>
#include <sys/stat.h>
#include <vector>

auto safe_file_size(const std::filesystem::path &p) -> uint64_t {
  try {
    return std::filesystem::file_size(p);
  } catch (const std::filesystem::filesystem_error &) {
    return UINT64_MAX;
  }
}

auto Atime(const std::string &path) -> uint64_t {
  struct stat st;
  if (stat(path.c_str(), &st) == 0) {
    return st.st_atime;
  } else {
    return 0;
  }
}

auto Mtime(const std::string &path) -> uint64_t {
  struct stat st;
  if (stat(path.c_str(), &st) == 0) {
    return st.st_mtime;
  } else {
    return 0;
  }
}

auto Ctime(const std::string &path) -> uint64_t {
  struct stat st;
  if (stat(path.c_str(), &st) == 0) {
    return st.st_ctime;
  } else {
    return 0;
  }
}

auto get_inode(const std::filesystem::path &p) -> uint64_t {
  struct stat st;
  if (stat(p.c_str(), &st) == 0) {
    return st.st_ino;
  } else {
    return UINT64_MAX; // 错误或不存在，使用 UINT64_MAX 作为错误哨兵，避免与合法
                       // inode(0) 混淆
  }
}

auto hash_data(const char *data, size_t size) -> uint64_t {
  XXH64_state_t *state = XXH64_createState();
  if (!state) {
    return 0; // 创建失败
  }
  XXH64_reset(state, 42); // 使用固定种子
  XXH64_update(state, data, size);
  uint64_t hash = XXH64_digest(state);
  XXH64_freeState(state);
  return hash;
}

auto hash_file_xxh64(const std::filesystem::path &p,
                     uint64_t seed = 42) -> uint64_t {
  try {
    std::ifstream ifs(p, std::ios::binary);
    if (!ifs)
      return 0;

    constexpr std::size_t kBufSz = 64 * 1024;
    char buf[kBufSz];

    // 使用 unique_ptr + 自定义删除器 确保 state 总被释放
    XXH64_state_t *raw = XXH64_createState();
    if (!raw)
      return 0;
    std::unique_ptr<XXH64_state_t, void (*)(XXH64_state_t *)> state(
        raw, [](XXH64_state_t *s) {
          if (s)
            XXH64_freeState(s);
        });

    XXH64_reset(state.get(), seed);

    while (ifs) {
      ifs.read(buf, kBufSz);
      std::streamsize n = ifs.gcount();
      if (n > 0) {
        XXH64_update(state.get(), buf, static_cast<size_t>(n));
      }
      // 如果出现严重 I/O 错误（badbit），返回失败（0）
      if (ifs.bad())
        return 0;
    }

    return XXH64_digest(state.get());
  } catch (const std::exception &) {
    return 0;
  } catch (...) {
    return 0;
  }
}

auto legal_file(const std::filesystem::path &f) -> bool {
  if (!std::filesystem::exists(f)) {
    return false;
  }
  if (!std::filesystem::is_regular_file(f)) {
    return false;
  }
  return true;
}

auto checkFileEqualDeep(const std::filesystem::path &p1,
                        const std::filesystem::path &p2,
                        bool &state_ok) -> int {
  state_ok = true;
  if (p1 == p2) {
    return 0;
  }
  if (!legal_file(p1) || !legal_file(p2)) {
    state_ok = false;
    return -1;
  }

  auto total_size = safe_file_size(p1);
  auto total_size1 = safe_file_size(p2);

  if (std::max(total_size, total_size1) == UINT64_MAX) {
    state_ok = false;
    return -1; // 无法获取文件大小
  }

  if (total_size < total_size1) {
    state_ok = false;
    return -1;
  } else if (total_size > total_size1) {
    state_ok = false;
    return 1;
  }

  std::ifstream ifs1(p1, std::ios::binary);
  std::ifstream ifs2(p2, std::ios::binary);
  if (!ifs1 || !ifs2) {
    state_ok = false;
    return -1;
  }

  constexpr std::size_t kBufSz = 64 * 1024;
  std::vector<char> buf1(kBufSz), buf2(kBufSz);

  while (ifs1 && ifs2) {
    ifs1.read(buf1.data(), static_cast<std::streamsize>(kBufSz));
    ifs2.read(buf2.data(), static_cast<std::streamsize>(kBufSz));
    std::streamsize n1 = ifs1.gcount();
    std::streamsize n2 = ifs2.gcount();

    if (n1 != n2) {
      state_ok = false;
      return -1;
    }

    int c = 0;
    if (n1 > 0 && (c = std::memcmp(buf1.data(), buf2.data(),
                                   static_cast<size_t>(n1))) != 0) {
      return c;
    }
  }

  // 最后检查是否在读取过程中发生了截断/增长
  if (safe_file_size(p1) != total_size || safe_file_size(p2) != total_size) {
    state_ok = false;
    return -1;
  }

  return 0;
}

DuplicateFinder::DuplicateFinder(std::filesystem::path dir,
                                 std::ofstream &output_json_path)
    : search_dir_(std::move(dir)), output_(output_json_path) {}

void DuplicateFinder::process() {
  process(oj_);
  output_ << oj_.dump(2);
}

class MyFile {
public:
  MyFile(const std::filesystem::path &p, bool &ok) : real_path_(p) {
    ok = true;
    if (!legal_file(p)) {
      ok = false;
    } else {
      atime_ = Atime(p.string());
      mtime_ = Mtime(p.string());
      ctime_ = Ctime(p.string());
      size_ = safe_file_size(p);

      if (std::min({atime_, mtime_, ctime_}) == 0) {
        ok = false;
      }
      if (size_ == 0 || size_ == UINT64_MAX) {
        ok = false;
      }
    }
  }

  auto compare(const MyFile &other) const -> int {
    if (size_ != other.size_) {
      return size_ < other.size_ ? -1 : 1;
    }

    set_first_4k();
    other.set_first_4k();

    auto compare_first_4k =
        memcmp(first_4k_data_, other.first_4k_data_, sizeof(first_4k_data_));
    if (compare_first_4k != 0) {
      return compare_first_4k;
    }

    hash();
    other.hash();

    if (hashed_ != other.hashed_) {
      return hashed_ < other.hashed_ ? -1 : 1;
    }

    bool ok = false;
    return checkFileEqualDeep(real_path_, other.real_path_, ok);
  }

  auto changed() const -> bool {
    if (!legal_file(real_path_)) {
      return true;
    }
    auto atime = Atime(real_path_.string());
    if (atime_ != atime) {
      return true;
    }
    auto mtime = Mtime(real_path_.string());
    if (mtime_ != mtime) {
      return true;
    }
    auto ctime = Ctime(real_path_.string());
    if (ctime_ != ctime) {
      return true;
    }
    auto size = safe_file_size(real_path_.string());
    if (size_ != size) {
      return true;
    }
    return false;
  }

private:
  friend class DuplicateFinder;
  std::filesystem::path real_path_;
  uint64_t size_ = 0;

  uint64_t atime_ = 0;
  uint64_t mtime_ = 0;
  uint64_t ctime_ = 0;

  mutable bool first_4k_ = false;
  mutable char first_4k_data_[4096] = {0};

  mutable bool hashed_ = false;
  mutable uint64_t hash_ = 0;

  auto set_first_4k() const -> bool {
    if (!first_4k_) {
      first_4k_ = true;
      std::cout << real_path_ << std::endl;
      std::ifstream ifs(real_path_, std::ios::binary);
      if (!ifs) {
        return false;
      }
      ifs.read(first_4k_data_, sizeof(first_4k_data_));
      return true;
    }
    return true;
  }

  auto hash() const -> uint64_t {
    if (!hashed_) {
      hashed_ = true;
      hash_ = hash_file_xxh64(real_path_);
    }
    return hash_;
  }

  auto little_hash() const -> uint64_t {
    set_first_4k();
    // 对前4kb哈希
    return hash_data(first_4k_data_, sizeof(first_4k_data_));
  }
};

void DuplicateFinder::process(nlohmann::json &output) {
  std::stack<std::filesystem::path> s;
  s.push(search_dir_);

  std::map<std::pair<uint64_t, uint64_t>, std::vector<MyFile>> all_files;
  {
    std::set<uint64_t> checked_inodes;
    while (!s.empty()) {
      auto c = s.top();
      s.pop();

      auto dir_str = c.string();
      if (dir_str.find("xzfs_fuse_tmp") != std::string::npos) {
        continue; // 忽略 xzfs_fuse_tmp 目录
      }

      // 遍历目录中的文件
      try {
        for (const auto &entry : std::filesystem::directory_iterator(c)) {
          auto name = entry.path().filename().string();
          if (!name.empty() && name[0] == '.') {
            continue; // 忽略以 '.' 开头的目录
          }

          if (entry.is_directory()) {
            s.push(entry.path());
          } else if (entry.is_regular_file()) {
            bool ok = false;
            MyFile f(entry.path(), ok);
            if (!ok) {
              continue;
            }

            // 获取 inode 编号；如果 stat
            // 失败则跳过该文件（避免将错误值插入去重集合）
            auto inode = get_inode(entry.path());
            if (inode == UINT64_MAX) {
              continue; // 无法获取 inode，跳过
            }
            if (checked_inodes.count(inode) > 0) {
              continue;
            }
            checked_inodes.insert(inode);

            auto little_hash = f.little_hash();
            auto size = f.size_;

            all_files[std::make_pair(little_hash, size)].push_back(f);
          }
        }
      } catch (const std::filesystem::filesystem_error &) {
        continue;
      }
    }
  }
  for (const auto &i : all_files) {
    if (i.second.size() > 1) {
      auto &maybe_equal_files = i.second;
      auto c = maybe_equal_files.size();
      std::map<uint64_t, bool> checked;
      std::vector<std::vector<std::filesystem::path>> same_file_groups;

      for (int i = 0; i < c; ++i) {
        if (checked[i]) {
          continue;
        }

        if (maybe_equal_files[i].changed()) {
          checked[i] = true;
          continue;
        }

        same_file_groups.emplace_back();
        same_file_groups.back().push_back(maybe_equal_files[i].real_path_);

        for (int j = 0; j < c; ++j) {
          if (j == i) {
            continue;
          }
          if (checked[j]) {
            continue;
          }

          if (maybe_equal_files[j].changed()) {
            checked[j] = true;
            continue;
          } else {
            if (maybe_equal_files[i].changed()) {
              same_file_groups.pop_back();
              break;
            } else {
              if (maybe_equal_files[i].compare(maybe_equal_files[j]) == 0) {
                same_file_groups.back().push_back(
                    maybe_equal_files[j].real_path_);
                checked[j] = true;
              }
            }
          }
        }
        for (const auto &group : same_file_groups) {
          if (group.size() > 1) {
            nlohmann::json item;
            for (const auto &path : group) {
              item.push_back({
                  {"display_path",
                   std::filesystem::relative(path, search_dir_).string()},
                  {"real_path", path.string()},
              });
            }
            output["items"].push_back(item);
          }
        }
      }
    }
  }
}
