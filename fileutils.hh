#ifndef FILEUTILS_HH
#define FILEUTILS_HH

#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <algorithm>
#include <functional>
#include <cstdint>

namespace
{
  template<class T>
  T convertEndianness(T number)
  {
    T res;
    unsigned char *bytes = reinterpret_cast<std::uint8_t*>(&number);
    for(int i = 0; i < sizeof(T); i++)
    {
      res <<= 8;
      res |= bytes[i];
    }
    return res;
  }

  template<class T>
  void writeNumber(std::ostream &out, T number)
  {
    number = convertEndianness(number);
    out.write(reinterpret_cast<char*>(&number), sizeof(T));
  }

  template<>
  void writeNumber(std::ostream &out, std::uint8_t number)
  {
    out.put(number);
  }

  template<class T>
  T readNumber(std::istream &in)
  {
    T res;
    in.read(reinterpret_cast<char*>(&res), sizeof(T));
    return convertEndianness(res);
  }

  template<>
  std::uint8_t readNumber(std::istream &in)
  {
    return std::uint8_t(in.get());
  }

  void writeString(std::ostream &out, const std::string &str)
  {
    out.write(str.c_str(), str.size());
    out.put('\0');
  }

  std::string readString(std::istream &in)
  {
    std::string res;
    for(char ch = in.get(); ch != '\0'; ch = in.get())
      res.push_back(ch);
    return res;
  }

  std::uint64_t copyFile(std::istream &in,
                         std::ostream &out,
                         std::uint64_t nbytes = std::numeric_limits<std::uint64_t>::max())
  {
    std::uint64_t ncopied;
    for(ncopied = 0; ncopied < nbytes; ++ncopied)
    {
      auto ch = in.get();
      if(in.eof())
        break;
      out.put(ch);
    }
    return ncopied;
  }
}

#endif

