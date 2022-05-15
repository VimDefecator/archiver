#include <iostream>
#include <iomanip>
#include <stack>
#include <string>

#include "archive.hh"

using namespace std;

int main(int argc, char **argv)
{
  auto archive = Archive(string(argv[1]));
  auto currentFolder = archive.getRootFolder();

  while(true)
  {
    string command;
    cout << "> ";
    cin >> command;
    if(command == "list")
    {
      currentFolder->iterChildren([](auto &entry)
      {
        cout << setw(8) << left
             << (entry.getType() == Entry::Type::File ? "FILE:" : "FOLDER:")
             << entry.getName()
             << endl;
      });
    }
    else if(command == "cd")
    {
      string name;
      cin >> name;
      if(name == "..")
      {
        currentFolder = currentFolder->getParentFolder();
      }
      else
      {
        auto success = currentFolder->iterChildrenUntil([&](auto &entry)
        {
          if(entry.getType() == Entry::Type::Folder && entry.getName() == name)
          {
            currentFolder = currentFolder->getChildFolder(entry);
            return true;
          }
          return false;
        });
        if(!success)
          cout << "not found!\n";
      }
    }
    else if(command == "addFolder")
    {
      string name;
      cin >> name;
      currentFolder->addFolder(name);
    }
    else if(command == "addFile")
    {
      string name, path;
      cin >> name >> path;
      currentFolder->addFile(name, path);
    }
    else if(command == "extract")
    {
      string name, path;
      cin >> name >> path;
      auto success = currentFolder->iterChildrenUntil([&](auto &entry)
      {
        if(entry.getType() == Entry::Type::File && entry.getName() == name)
        {
          currentFolder->extract(entry, path);
          return true;
        }
        return false;
      });
      if(!success)
        cout << "not found!\n";
    }
    else if(command == "remove")
    {
      string name;
      cin >> name;
      auto success = currentFolder->iterChildrenUntil([&](auto &entry)
      {
        if(entry.getName() == name)
        {
          currentFolder->remove(entry);
          return true;
        }
        return false;
      });
      if(!success)
        cout << "not found!\n";
    }
    else if(command == "sync")
    {
      archive.sync();
      currentFolder = archive.getRootFolder();
    }
  }
}

