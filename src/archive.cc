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
  : path_(move(path))
{
  file_.open(path_, ios_base::binary | ios_base::in | ios_base::out);
  if(file_.is_open() && (file_.seekg(0, ios_base::end).tellg() != 0))
  {
    loadFolders();
  }
  else
  {
    file_.open(path_, ios_base::binary | ios_base::in | ios_base::out | ios_base::trunc);
    addFolder(0);
  }
}

uint16_t Archive::addFolder(uint16_t parentIndex)
{
  int i = 0;
  while(i < folders_.size() && !folders_[i].deleted_)
    ++i;

  if(i < folders_.size())
    folders_[i].deleted_ = false;
  else
    folders_.emplace_back(this, i);

  folders_[i].parentIndex_ = parentIndex;
  folders_[i].created_ = true;
  return i;
}

void Archive::removeFolder(uint16_t index)
{
  auto &folder = folders_[index];
  if(!folder.created_)
    deletedFolders_.push_back(exchange(folder, Folder(this, index)));
  else
    folder = Folder(this, index);
  folder.deleted_ = true;
}

void Archive::loadFolders()
{
  file_.seekg(folderTableOffset(0), ios_base::end);

  auto numFolders = readNumber<uint16_t>(file_);
  endDataOffset_ = readNumber<uint64_t>(file_);

  file_.seekg(folderTableOffset(numFolders), ios_base::end);

  auto folderOffsets = vector<int32_t>(numFolders);
  for(auto &offset : folderOffsets)
    offset = readNumber<int32_t>(file_);

  folders_.reserve(numFolders);
  for(int i = 0; i < folderOffsets.size(); ++i)
  {
    folders_.emplace_back(this, i);
    auto &folder = folders_.back();
    file_.seekg(folderOffsets[i], ios_base::end);
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
  if(!synced_)
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
    if(!folder.synced_)
      for(auto &entry : folder.entries_)
        if(entry.type == Entry::Type::File && entry.deleted)
          skippedIntervals.emplace_back(entry.offset, entry.offset + entry.size);
  };

  for(auto &folder : folders_)
    if(!folder.deleted_ && !folder.created_)
      collectDeleted(folder);

  for(auto &folder : deletedFolders_)
    collectDeleted(folder);

  if(!skippedIntervals.empty())
  {
    sort(skippedIntervals.begin(), skippedIntervals.end(),
         [](auto interval1, auto interval2){return interval1.first < interval2.first;});
    skippedIntervals.emplace_back(endDataOffset_, endDataOffset_);
    auto numIntervals = skippedIntervals.size();

    auto oldFile = fstream(path_, ios_base::binary | ios_base::in);
    file_.seekp(skippedIntervals[0].first);

    for(int i = 1; i < numIntervals; ++i)
    {
      auto prevSkipTo = skippedIntervals[i-1].second;
      auto nextSkipFrom = skippedIntervals[i].first;

      oldFile.seekg(prevSkipTo);
      copyFile(oldFile, file_, nextSkipFrom - prevSkipTo);
    }

    auto intervalSums = vector<uint64_t>(numIntervals, 0);
    for(int i = 1; i < numIntervals; ++i)
      intervalSums[i] = intervalSums[i-1] + skippedIntervals[i].second - skippedIntervals[i].first;

    for(auto &folder : folders_)
      if(!folder.deleted_ && !folder.created_)
        for(auto &entry : folder.entries_)
          if(entry.type == Entry::Type::File && !entry.deleted && !entry.created)
          {
            auto nextIntervalIndex = upper_bound(skippedIntervals.begin(), skippedIntervals.end(), entry.offset,
                                                 [](auto offset, auto interval){return offset < interval.second;})
                                     - skippedIntervals.begin();
            entry.offset -= intervalSums[nextIntervalIndex-1];
          }

    endDataOffset_ = file_.tellp();
  }
}

void Archive::syncAddFiles()
{
  for(auto &folder : folders_)
    if(!folder.deleted_ && !folder.synced_)
      for(auto &entry : folder.entries_)
        if(entry.type == Entry::Type::File && entry.created)
        {
          auto externalFile = fstream(entry.externalPath, ios_base::binary | ios_base::in);
          entry.offset = file_.tellp();
          entry.size = copyFile(externalFile, file_);
        }

  endDataOffset_ = file_.tellp();
}

void Archive::syncFolders()
{
  deletedFolders_.clear();

  while(folders_.back().deleted_)
    folders_.pop_back();

  auto folderIndexRemap = vector<uint16_t>(folders_.size());
  for(int i = 0; i < folders_.size(); ++i)
  {
    if(folders_[i].deleted_)
    {
      folders_[i] = move(folders_.back());
      folders_.pop_back();
      folderIndexRemap[folders_.size()] = i;

      while(folders_.back().deleted_)
        folders_.pop_back();
    }
    else
    {
      folderIndexRemap[i] = i;
    }
  }

  auto numFolders = uint16_t(folders_.size());
  auto folderRelativeOffsets = vector<int32_t>(numFolders);
  auto foldersSegmentOffset = file_.tellp();

  for(int i = 0; i < numFolders; ++i)
  {
    auto &folder = folders_[i];
    folder.selfIndex_ = i;
    folder.parentIndex_ = folderIndexRemap[folder.parentIndex_];
    for(auto it = folder.entries_.begin();
        it != folder.entries_.end();)
    {
      if(it->deleted)
      {
        it = folder.entries_.erase(it);
      }
      else
      {
        if(it->type == Entry::Type::Folder)
          it->offset = folderIndexRemap[it->offset];
        ++it;
      }
    }

    folderRelativeOffsets[i] = int32_t(int64_t(file_.tellp()) - int64_t(foldersSegmentOffset));
    folder.write();
  }

  auto foldersSegmentLength = file_.tellp() - foldersSegmentOffset;

  for(auto &relativeOffset : folderRelativeOffsets)
  {
    auto off = int32_t(folderTableOffset(numFolders) - foldersSegmentLength + relativeOffset);
    writeNumber(file_, off);
  }

  writeNumber(file_, numFolders);
  writeNumber(file_, endDataOffset_);
}

void Archive::syncSize()
{
  auto newSize = file_.tellp();
  file_.close();
  filesystem::resize_file(path_, newSize);
  file_.open(path_, ios_base::binary | ios_base::in | ios_base::out);
}

void Archive::setSynced(bool synced)
{
  if((synced_ = synced))
    for(auto &folder : folders_)
      folder.setSynced(true);
}

Folder::Folder(Archive *archive, uint16_t selfIndex)
  : archive_(archive)
  , selfIndex_(selfIndex)
{
}

FolderHandle Folder::getChildFolder(const Entry &entry)
{
  if(entry.type != Entry::Type::Folder)
    throw runtime_error("Folder::getChildFolder: Not a folder!");

  return FolderHandle(archive_, entry.offset);
}

FolderHandle Folder::getParentFolder()
{
  return FolderHandle(archive_, parentIndex_);
}

void Folder::addFolder(string name)
{
  // 'archive_->addFolder' may realloc folders container and invalidate 'this'
  auto self = FolderHandle(archive_, selfIndex_);

  Entry entry;
  entry.type = Entry::Type::Folder;
  entry.name = move(name);
  entry.offset = archive_->addFolder(selfIndex_);
  entry.created = true;

  self->entries_.push_back(move(entry));
  self->setSynced(false);
}

void Folder::addFile(string name, string externalPath)
{
  Entry entry;
  entry.type = Entry::Type::File;
  entry.name = move(name);
  entry.externalPath = move(externalPath);
  entry.created = true;
  entries_.push_back(move(entry));
  setSynced(false);
}

void Folder::remove(const Entry &entry)
{
  remove(&entry - &entries_[0]);
}

void Folder::extract(const Entry &entry, string path)
{
  if(entry.type != Entry::Type::File)
    throw runtime_error("Folder::extract: Not a file!");

  if(entry.created)
    throw runtime_error("Folder::extract: Not added to archive yet!");

  archive_->file_.seekg(entry.offset);
  auto file = ofstream(path);
  copyFile(archive_->file_, file, entry.size);
}

void Folder::remove(uint16_t index)
{
  auto &entry = entries_[index];

  if(entry.type == Entry::Type::Folder)
  {
    auto &childFolder = archive_->folders_[entry.offset];
    for(int i = childFolder.entries_.size() - 1; i >= 0; --i)
      childFolder.remove(i);

    archive_->removeFolder(entry.offset);
  }

  if(entry.created)
    entries_.erase(entries_.begin() + index);
  else
    entry.deleted = true;

  setSynced(false);
}

void Folder::read()
{
  parentIndex_ = readNumber<uint16_t>(archive_->file_);
  auto numEntries = readNumber<uint16_t>(archive_->file_);
  entries_ = vector<Entry>(numEntries);
  for(auto &entry : entries_)
    entry.read(archive_->file_);
}

void Folder::write()
{
  writeNumber(archive_->file_, parentIndex_);
  writeNumber(archive_->file_, uint16_t(entries_.size()));
  for(auto &entry : entries_)
    entry.write(archive_->file_);
}

void Folder::setSynced(bool synced)
{
  if(!(synced_ = synced))
    archive_->setSynced(false);
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

