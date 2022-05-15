#ifndef ARCHIVE_HH
#define ARCHIVE_HH

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <functional>
#include <cstdint>

class Folder;
class Entry;
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

  std::string m_path;
  std::fstream m_file;
  std::uint64_t m_endDataOffset;
  std::vector<Folder> m_folders;
  std::vector<Folder> m_deletedFolders;
  bool m_synced;

  friend class Folder;
  friend class FolderHandle;
};

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
};

class Folder
{
public:
  Folder(Archive *archive, std::uint16_t selfIndex);

  void iterChildren(std::function<void(Entry&)> fn)
  {
    for(auto &entry : m_entries)
      if(!entry.deleted)
        fn(entry);
  }

  bool iterChildrenUntil(std::function<bool(Entry&)> fn)
  {
    for(auto &entry : m_entries)
      if(!entry.deleted)
        if(fn(entry))
          return true;
    return false;
  }

  FolderHandle getChildFolder(Entry &entry);
  FolderHandle getParentFolder();
  void addFolder(std::string name);
  void addFile(std::string name, std::string path);
  void remove(Entry &entry);
  void extract(Entry &entry, std::string path);

private:
  void remove(std::uint16_t index);
  void read();
  void write();
  void setSynced(bool synced);

  Archive *m_archive;
  std::uint16_t m_selfIndex;
  std::uint16_t m_parentIndex;
  std::vector<Entry> m_entries;
  bool m_created = false;
  bool m_deleted = false;
  bool m_synced = false;

  friend class Archive;
};

class FolderHandle
{
public:
  FolderHandle(Archive *archive, std::uint16_t folderIndex)
    : m_archive(archive)
    , m_folderIndex(folderIndex)
  {
  }

  Folder *operator->()
  {
    return &m_archive->m_folders[m_folderIndex];
  }

private:
  Archive *m_archive;
  std::uint16_t m_folderIndex;
};

#endif

