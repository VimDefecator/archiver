#ifndef ARCHIVE_HH
#define ARCHIVE_HH

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <functional>
#include <cstdint>
#include <iterator>

class Folder;
class FolderHandle;

class Archive
{
public:
  Archive(std::string path);
  FolderHandle getRootFolder();
  void sync();

private:
  std::uint16_t addFolder(std::uint16_t parentIndex);
  void removeFolder(std::uint16_t index);
  void loadFolders();
  void syncRemoveFiles();
  void syncAddFiles();
  void syncFolders();
  void syncSize();
  void setSynced(bool synced);

  std::string path_;
  std::fstream file_;
  std::uint64_t endDataOffset_;
  std::vector<Folder> folders_;
  std::vector<Folder> deletedFolders_;
  bool synced_;

  friend class Folder;
  friend class FolderHandle;
};

class Entries;

class Entry
{
public:
  enum class Type { File = 0, Folder = 1 };

  Type getType() const { return type; }
  const std::string &getName() const { return name; }
  std::uint64_t getSize() const { return size; };

private:
  void read(std::istream &in);
  void write(std::ostream &out);

  Type type;
  std::string name, externalPath;
  std::uint64_t offset, size = 0;
  bool created = false;
  bool deleted = false;

  friend class Folder;
  friend class Archive;
  friend class Entries;
};

class Entries
{
public:
  Entries(const std::vector<Entry> &entries)
    : entries_(entries)
  {
  }

  class Iterator
  {
  public:
    using iterator_category = std::input_iterator_tag;
    using difference_type   = std::vector<Entry>::const_iterator::difference_type;
    using value_type        = std::vector<Entry>::const_iterator::value_type;
    using pointer           = std::vector<Entry>::const_iterator::pointer;
    using reference         = std::vector<Entry>::const_iterator::reference;

    Iterator(std::vector<Entry>::const_iterator it,
             std::vector<Entry>::const_iterator endIt)
      : it_(it), endIt_(endIt)
    {
    }

    bool operator!=(const Iterator &other) {
      return it_ != other.it_;
    }
    const Entry& operator*() {
      return *it_;
    }
    const Entry& operator->() {
      return *it_;
    }
    Iterator operator++() {
      do ++it_; while(it_ != endIt_ && it_->deleted);
      return *this;
    }
    Iterator operator++(int) {
      auto prev = *this;
      ++*this;
      return prev;
    }

  private:
    std::vector<Entry>::const_iterator it_, endIt_;
  };

  Iterator begin() const {
    return Iterator(entries_.begin(), entries_.end());
  }
  Iterator end() const {
    return Iterator(entries_.end(), entries_.end());
  }

private:
  const std::vector<Entry> &entries_;
};

class Folder
{
public:
  Folder(Archive *archive, std::uint16_t selfIndex);

  Entries getChildren() { return Entries(entries_); }

  FolderHandle getChildFolder(const Entry &entry);
  FolderHandle getParentFolder();
  void addFolder(std::string name);
  void addFile(std::string name, std::string path);
  void remove(const Entry &entry);
  void extract(const Entry &entry, std::string path);

private:
  void remove(std::uint16_t index);
  void read();
  void write();
  void setSynced(bool synced);

  Archive *archive_;
  std::uint16_t selfIndex_;
  std::uint16_t parentIndex_;
  std::vector<Entry> entries_;
  bool created_ = false;
  bool deleted_ = false;
  bool synced_ = false;

  friend class Archive;
};

class FolderHandle
{
public:
  FolderHandle(Archive *archive, std::uint16_t folderIndex)
    : archive_(archive)
    , folderIndex_(folderIndex)
  {
  }

  Folder *operator->()
  {
    return &archive_->folders_[folderIndex_];
  }

private:
  Archive *archive_;
  std::uint16_t folderIndex_;
};

#endif
