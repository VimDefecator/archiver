#include <iostream>
#include <iomanip>
#include <stack>
#include <string>
#include <algorithm>

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
      for(const auto &entry : currentFolder->getChildren())
      {
        cout << setw(8) << left
             << (entry.getType() == Entry::Type::File ? "FILE:" : "FOLDER:")
             << entry.getName()
             << endl;
      }
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
        bool success = false;

        for(const auto &entry : currentFolder->getChildren())
        {
          if(entry.getType() == Entry::Type::Folder && entry.getName() == name)
          {
            currentFolder = currentFolder->getChildFolder(entry);
            success = true;
            break;
          }
        }

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

      bool success = false;

      for(const auto &entry : currentFolder->getChildren())
      {
        if(entry.getType() == Entry::Type::File && entry.getName() == name)
        {
          currentFolder->extract(entry, path);
          success = true;
          break;
        }
      }

      if(!success)
        cout << "not found!\n";
    }
    else if(command == "remove")
    {
      string name;
      cin >> name;

      bool success = false;
      
      for(const auto &entry : currentFolder->getChildren())
      {
        if(entry.getName() == name)
        {
          currentFolder->remove(entry);
          success = true;
          break;
        }
      }

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

