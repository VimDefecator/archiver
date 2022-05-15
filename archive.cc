#include <algorithm>
#include <utility>
#include <stdexcept>
#include <filesystem>

#include "archive.hh"
#include "fileutils.hh"

using namespace std;

static constexpr int32_t folderTableOffset(int numFolders)
{
  return -(sizeof(uint64_t) + sizeof(uint16_t) + numFolders*sizeof(int32_t));
}

Archive::Archive(string path)
  : m_path(move(path))
{
  m_file.open(m_path, ios_base::binary | ios_base::in | ios_base::out);
  if(m_file.is_open())
  {
    loadFolders();
  }
  else
  {
    m_file.open(m_path, ios_base::binary | ios_base::in | ios_base::out | ios_base::trunc);
    addFolder(0);
  }
}

uint16_t Archive::addFolder(uint16_t parentIndex)
{
  int i = 0;
  while(i < m_folders.size() && !m_folders[i].m_deleted)
    ++i;

  if(i < m_folders.size())
    m_folders[i].m_deleted = false;
  else
    m_folders.emplace_back(this, i);

  m_folders[i].m_parentIndex = parentIndex;
  m_folders[i].m_created = true;
  return i;
}

void Archive::removeFolder(uint16_t index)
{
  auto &folder = m_folders[index];
  if(!folder.m_created)
    m_deletedFolders.push_back(exchange(folder, Folder(this, index)));
  else
    folder = Folder(this, index);
  folder.m_deleted = true;
}

void Archive::loadFolders()
{
  m_file.seekg(folderTableOffset(0), ios_base::end);

  auto numFolders = readNumber<uint16_t>(m_file);
  m_endDataOffset = readNumber<uint64_t>(m_file);

  m_file.seekg(folderTableOffset(numFolders), ios_base::end);

  auto folderOffsets = vector<int32_t>(numFolders);
  for(auto &offset : folderOffsets)
    offset = readNumber<int32_t>(m_file);

  m_folders.reserve(numFolders);
  for(int i = 0; i < folderOffsets.size(); ++i)
  {
    m_folders.emplace_back(this, i);
    auto &folder = m_folders.back();
    m_file.seekg(folderOffsets[i], ios_base::end);
    folder.read();
  }

  setSynced(true);
}

FolderHandle Archive::getRootFolder()
{
  return FolderHandle(this, 0);
}

void Archive::sync()
{
  if(!m_synced)
  {
    syncRemoveFiles();
    syncAddFiles();
    syncFolders();
    syncSize();
    setSynced(true);
  }
}

void Archive::syncRemoveFiles()
{
  vector<pair<uint64_t,uint64_t>> skippedIntervals;

  auto collectDeleted = [&](Folder &folder)
  {
    if(!folder.m_synced)
      for(auto &entry : folder.m_entries)
        if(entry.type == Entry::Type::File && entry.deleted)
          skippedIntervals.emplace_back(entry.offset, entry.offset + entry.size);
  };

  for(auto &folder : m_folders)
    if(!folder.m_deleted && !folder.m_created)
      collectDeleted(folder);

  for(auto &folder : m_deletedFolders)
    collectDeleted(folder);

  if(!skippedIntervals.empty())
  {
    sort(skippedIntervals.begin(), skippedIntervals.end(),
         [](auto interval1, auto interval2){return interval1.first < interval2.first;});
    skippedIntervals.emplace_back(m_endDataOffset, m_endDataOffset);
    auto numIntervals = skippedIntervals.size();

    auto oldFile = fstream(m_path, ios_base::binary | ios_base::in);
    m_file.seekp(skippedIntervals[0].first);

    for(int i = 1; i < numIntervals; ++i)
    {
      auto prevSkipTo = skippedIntervals[i-1].second;
      auto nextSkipFrom = skippedIntervals[i].first;

      oldFile.seekg(prevSkipTo);
      copyFile(oldFile, m_file, nextSkipFrom - prevSkipTo);
    }

    auto intervalSums = vector<uint64_t>(numIntervals, 0);
    for(int i = 1; i < numIntervals; ++i)
      intervalSums[i] = intervalSums[i-1] + skippedIntervals[i].second - skippedIntervals[i].first;

    for(auto &folder : m_folders)
      if(!folder.m_deleted && !folder.m_created)
        for(auto &entry : folder.m_entries)
          if(entry.type == Entry::Type::File && !entry.deleted && !entry.created)
          {
            auto nextIntervalIndex = upper_bound(skippedIntervals.begin(), skippedIntervals.end(), entry.offset,
                                                 [](auto offset, auto interval){return offset < interval.second;})
                                     - skippedIntervals.begin();
            entry.offset -= intervalSums[nextIntervalIndex-1];
          }

    m_endDataOffset = m_file.tellp();
  }
}

void Archive::syncAddFiles()
{
  for(auto &folder : m_folders)
    if(!folder.m_deleted && !folder.m_synced)
      for(auto &entry : folder.m_entries)
        if(entry.type == Entry::Type::File && entry.created)
        {
          auto externalFile = fstream(entry.externalPath, ios_base::binary | ios_base::in);
          entry.offset = m_file.tellp();
          entry.size = copyFile(externalFile, m_file);
        }

  m_endDataOffset = m_file.tellp();
}

void Archive::syncFolders()
{
  m_deletedFolders.clear();

  while(m_folders.back().m_deleted)
    m_folders.pop_back();

  auto folderIndexRemap = vector<uint16_t>(m_folders.size());
  for(int i = 0; i < m_folders.size(); ++i)
  {
    if(m_folders[i].m_deleted)
    {
      m_folders[i] = move(m_folders.back());
      m_folders.pop_back();
      folderIndexRemap[m_folders.size()] = i;

      while(m_folders.back().m_deleted)
        m_folders.pop_back();
    }
    else
    {
      folderIndexRemap[i] = i;
    }
  }

  auto numFolders = uint16_t(m_folders.size());
  auto folderRelativeOffsets = vector<int32_t>(numFolders);
  auto foldersSegmentOffset = m_file.tellp();

  for(int i = 0; i < numFolders; ++i)
  {
    auto &folder = m_folders[i];
    folder.m_selfIndex = i;
    folder.m_parentIndex = folderIndexRemap[folder.m_parentIndex];
    for(auto it = folder.m_entries.begin();
        it != folder.m_entries.end();)
    {
      if(it->deleted)
      {
        it = folder.m_entries.erase(it);
      }
      else
      {
        if(it->type == Entry::Type::Folder)
          it->offset = folderIndexRemap[it->offset];
        ++it;
      }
    }

    folderRelativeOffsets[i] = int32_t(int64_t(m_file.tellp()) - int64_t(foldersSegmentOffset));
    folder.write();
  }

  auto foldersSegmentLength = m_file.tellp() - foldersSegmentOffset;

  for(auto &relativeOffset : folderRelativeOffsets)
  {
    auto off = int32_t(folderTableOffset(numFolders) - foldersSegmentLength + relativeOffset);
    writeNumber(m_file, off);
  }

  writeNumber(m_file, numFolders);
  writeNumber(m_file, m_endDataOffset);
}

void Archive::syncSize()
{
  auto newSize = m_file.tellp();
  m_file.close();
  filesystem::resize_file(m_path, newSize);
  m_file.open(m_path, ios_base::binary | ios_base::in | ios_base::out);
}

void Archive::setSynced(bool synced)
{
  if((m_synced = synced))
    for(auto &folder : m_folders)
      folder.setSynced(true);
}

Folder::Folder(Archive *archive, uint16_t selfIndex)
  : m_archive(archive)
  , m_selfIndex(selfIndex)
{
}

FolderHandle Folder::getChildFolder(Entry &entry)
{
  if(entry.type != Entry::Type::Folder)
    throw runtime_error("Folder::getChildFolder: Not a folder!");

  return FolderHandle(m_archive, entry.offset);
}

FolderHandle Folder::getParentFolder()
{
  return FolderHandle(m_archive, m_parentIndex);
}

void Folder::addFolder(std::string name)
{
  // 'm_archive->addFolder' may realloc folders container and invalidate 'this'
  auto self = FolderHandle(m_archive, m_selfIndex);

  Entry entry;
  entry.type = Entry::Type::Folder;
  entry.name = move(name);
  entry.offset = m_archive->addFolder(m_selfIndex);
  entry.created = true;

  self->m_entries.push_back(move(entry));
  self->setSynced(false);
}

void Folder::addFile(std::string name, std::string externalPath)
{
  Entry entry;
  entry.type = Entry::Type::File;
  entry.name = move(name);
  entry.externalPath = move(externalPath);
  entry.created = true;
  m_entries.push_back(move(entry));
  setSynced(false);
}

void Folder::extract(Entry &entry, std::string path)
{
  if(entry.type != Entry::Type::File)
    throw runtime_error("Folder::extract: Not a file!");

  if(entry.created)
    throw runtime_error("Folder::extract: Not added to archive yet!");

  m_archive->m_file.seekg(entry.offset);
  auto file = ofstream(path);
  copyFile(m_archive->m_file, file, entry.size);
}

void Folder::remove(Entry &entry)
{
  remove(&entry - &m_entries[0]);
}

void Folder::remove(uint16_t index)
{
  auto &entry = m_entries[index];

  if(entry.type == Entry::Type::Folder)
  {
    auto &childFolder = m_archive->m_folders[entry.offset];
    for(int i = childFolder.m_entries.size() - 1; i >= 0; --i)
      childFolder.remove(i);

    m_archive->removeFolder(entry.offset);
  }

  if(entry.created)
    m_entries.erase(m_entries.begin() + index);
  else
    entry.deleted = true;

  setSynced(false);
}

void Folder::read()
{
  m_parentIndex = readNumber<uint16_t>(m_archive->m_file);
  auto numEntries = readNumber<uint16_t>(m_archive->m_file);
  m_entries = vector<Entry>(numEntries);
  for(auto &entry : m_entries)
    entry.read(m_archive->m_file);
}

void Folder::write()
{
  writeNumber(m_archive->m_file, m_parentIndex);
  writeNumber(m_archive->m_file, uint16_t(m_entries.size()));
  for(auto &entry : m_entries)
    entry.write(m_archive->m_file);
}

void Folder::setSynced(bool synced)
{
  if(!(m_synced = synced))
    m_archive->setSynced(false);
}

void Entry::read(istream &in)
{
  type = Type(readNumber<uint8_t>(in));
  if(type == Type::File)
  {
    offset = readNumber<uint64_t>(in);
    size = readNumber<uint64_t>(in);
  }
  else
  {
    offset = readNumber<uint32_t>(in);
  }
  name = readString(in);
}

void Entry::write(ostream &out)
{
  writeNumber(out, uint8_t(type));
  if(type == Type::File)
  {
    writeNumber(out, offset);
    writeNumber(out, size);
  }
  else
  {
    writeNumber(out, uint32_t(offset));
  }
  writeString(out, name);
}

