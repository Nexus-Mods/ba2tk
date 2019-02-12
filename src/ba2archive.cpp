#include "ba2archive.h"
#include "ba2exception.h"
#include "dds.h"
#include <cstring>
#include <fstream>
#include <algorithm>
#include <chrono>
#include <queue>
#include <memory>
#include <mutex>
#include <zlib.h>
#include <sys/stat.h>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

using std::fstream;
using namespace std::chrono_literals;


namespace BA2 {

Archive::Archive()
  : m_Type(TYPE_GENERAL)
{
}


Archive::~Archive()
{
  if (m_File.is_open()) {
    m_File.close();
  }
}


EType Archive::typeFromID(const char *typeID)
{
  if (strncmp(typeID, "GNRL", 4) == 0) {
    return TYPE_GENERAL;
  }
  else if (strncmp(typeID, "DX10", 4) == 0) {
    return TYPE_DX10;
  }
  else {
    throw data_invalid_exception(makeString("invalid type %d", typeID));
  }
}


const char *Archive::typeToID(EType type)
{
  switch (type) {
    case TYPE_GENERAL: return "GNRL";
    case TYPE_DX10: return "DX10";
    default: throw data_invalid_exception(makeString("invalid type %d", type));
  }
}


Archive::Header Archive::readHeader(std::fstream &infile)
{
  Header result;

  char fileID[4];
  infile.read(fileID, 4);
  if (memcmp(fileID, "BTDX", 4) != 0) {
    throw data_invalid_exception(makeString("not a ba2 file"));
  }

  result.version          = readType<BSAULong>(infile);
  char typeBuffer[5];
  infile.read(typeBuffer, 4);
  typeBuffer[4] = '\0';
  result.type          = typeFromID(typeBuffer);
  result.fileCount        = readType<BSAULong>(infile);
  result.offsetNameTable  = readType<BSAHash>(infile); 

  return result;
}


EErrorCode Archive::read(const char *fileName)
{
  m_File.open(fileName, fstream::in | fstream::binary);
  return read();
}

EErrorCode Archive::read(const wchar_t *fileName)
{
  m_File.open(fileName, fstream::in | fstream::binary);
  return read();
}

EErrorCode Archive::read() {
  if (!m_File.is_open()) {
    return ERROR_FILENOTFOUND;
  }
  m_File.exceptions(std::ios_base::badbit);
  try {
    try {
      m_Header = readHeader(m_File);
    } catch (const data_invalid_exception&) {
      return ERROR_INVALIDDATA;
    }

    m_Type = m_Header.type;

    if (m_Type == TYPE_GENERAL)
    {
      if (!readGeneral())
        return ERROR_INVALIDDATA;
    }
    else if (m_Type == TYPE_DX10)
    {
      if (!readDX10())
        return ERROR_INVALIDDATA;
    }

    if (!readNametable())
      return ERROR_INVALIDDATA;

    return ERROR_NONE;
  } catch (std::ios_base::failure&) {
    return ERROR_INVALIDDATA;
  }
}


bool Archive::readGeneral()
{
  m_Files.resize(m_Header.fileCount);
  if (m_Header.fileCount) {
    m_File.read((char*)&m_Files[0], sizeof(FileEntry) * m_Header.fileCount);
  }

  return true;
}


bool Archive::readDX10()
{
  m_Textures.resize(m_Header.fileCount);

  for(BSAULong i = 0; i < m_Textures.size(); i++)
  {
    Texture *texture = &m_Textures[i];
    m_File.read((char*)&texture->texhdr, sizeof(texture->texhdr));

    texture->texchunks.resize(texture->texhdr.numChunks);
    if(texture->texhdr.numChunks)
      m_File.read((char*)&texture->texchunks[0], sizeof(DX10Chunk) * texture->texhdr.numChunks);
  }

  return true;
}


bool Archive::readNametable()
{
  m_File.seekg(0, std::ios_base::end);
  size_t fileSize = m_File.tellg();
  m_File.seekg(m_Header.offsetNameTable);
  std::unique_ptr<char[]> buffer(new char[0x10000]);

  while((fileSize - m_File.tellg()) >= 2)
  {
    BSAULong length = readType<BSAUShort>(m_File);

    m_File.read(buffer.get(), length);
    buffer[length] = '\0';

    m_TableNames.push_back(std::string(buffer.get()));
  }

  return true;
}

void Archive::close()
{
  m_File.close();
}


BSAULong Archive::countFiles() const
{
  return m_Header.fileCount;
}


BSAULong Archive::countCharacters(const std::vector<std::string> &list) const
{
  size_t sum = 0;
  for (std::vector<std::string>::const_iterator iter = list.begin();
       iter != list.end(); ++iter) {
    sum += iter->length() + 1;
  }
  return static_cast<BSAULong>(sum);
}

#ifndef WIN32
#define _stricmp strcasecmp
#endif // WIN32

static bool endsWith(const std::string &fileName, const char *extension)
{
  size_t endLength = strlen(extension);
  if (fileName.length() < endLength) {
    return false;
  }
  return _stricmp(&fileName[fileName.length() - endLength], extension) == 0;
}


BSAULong Archive::determineFileFlags(const std::vector<std::string> &fileList) const
{
  BSAULong result = 0;

  bool hasDDS = false;

  for (std::vector<std::string>::const_iterator iter = fileList.begin();
       iter != fileList.end(); ++iter) {
     if (!hasDDS && endsWith(*iter, ".dds")) {
      hasDDS = true;
      result |= 1 << 1;
    }
  }
  return result;
}

std::vector<std::string> const Archive::getFileList()
{
  return m_TableNames;
}

EErrorCode Archive::extractAll(const char *destination,
                        const std::function<bool (int value, std::string fileName)> &progress,
                        bool overwrite) const
{
  switch (m_Header.type) {
    case TYPE_GENERAL: return extractAllGeneral(destination);
    case TYPE_DX10: return extractAllDX10(destination);
    default: return ERROR_INVALIDDATA;
  }
}
  

EErrorCode Archive::extractAllGeneral(const char *destination) const
{
  if (m_Files.size() != m_TableNames.size()) {
    return ERROR_INVALIDDATA;
  }

  for(BSAULong i = 0; i < m_Files.size(); ++i)
  {
    const FileEntry &file = m_Files[i];

    std::string destinationPath = std::string(destination) + "\\" + m_TableNames[i];

    std::fstream outFile;
    outFile.open(destinationPath.c_str(), fstream::out | fstream::binary);
    if (m_File.is_open()) {
      m_File.seekg(file.offset);

      if ((file.packedLen != 0) && (file.unpackedLen != file.packedLen)) {
        BSAULong unpackedLen = file.unpackedLen;
        if (!unpackedLen) {
          unpackedLen = file.unk20;	// ???
        }

        // TODO Umm, maybe don't read the whole thing in one go? Who knows how large
        //   this file could be. Do this in chunks like civilized people!
        std::unique_ptr<BSAUChar[]> sourceBuffer(new BSAUChar[file.packedLen]);

        m_File.read((char*)sourceBuffer.get(), file.packedLen);

        std::unique_ptr<BSAUChar[]> destinationBuffer(new BSAUChar[unpackedLen]);

        BSAULong bytesWritten = unpackedLen;
        int result = uncompress(destinationBuffer.get(), &bytesWritten, sourceBuffer.get(), file.packedLen);
        if ((result != Z_OK) || (bytesWritten != unpackedLen)) {
          return ERROR_INVALIDDATA;
        }

        outFile.write((const char*)destinationBuffer.get(), unpackedLen);
      }
      else {
        std::unique_ptr<BSAUChar[]> destinationBuffer(new BSAUChar[file.unpackedLen]);
        m_File.read((char*)destinationBuffer.get(), file.unpackedLen);
        outFile.write((const char*)destinationBuffer.get(), file.unpackedLen);
      }
    }
  }
  return ERROR_NONE;
}


EErrorCode Archive::extractAllDX10(const char *destination) const
{
  for(BSAULong i = 0; i < m_Textures.size(); ++i)
  {
    const Texture *texture = &m_Textures[i];

    std::string destinationPath = destination;
    destinationPath += "\\";
    destinationPath += m_TableNames[i];

    std::fstream outFile;
    outFile.open(destinationPath.c_str(), fstream::out | fstream::binary);
    if (m_File.is_open()) {
      DDS_HEADER ddsHeader = { 0 };

      ddsHeader.dwSize = sizeof(ddsHeader);
      ddsHeader.dwHeaderFlags = DDS_HEADER_FLAGS_TEXTURE | DDS_HEADER_FLAGS_LINEARSIZE | DDS_HEADER_FLAGS_MIPMAP;
      ddsHeader.dwHeight = texture->texhdr.height;
      ddsHeader.dwWidth = texture->texhdr.width;
      ddsHeader.dwMipMapCount = texture->texhdr.numMips;
      ddsHeader.ddspf.dwSize = sizeof(DDS_PIXELFORMAT);
      ddsHeader.dwSurfaceFlags = DDS_SURFACE_FLAGS_TEXTURE | DDS_SURFACE_FLAGS_MIPMAP;

      bool ok = true;

      switch(texture->texhdr.format)
      {
      case DXGI_FORMAT_BC1_UNORM:
        ddsHeader.ddspf.dwFlags = DDS_FOURCC;
        ddsHeader.ddspf.dwFourCC = MAKEFOURCC('D', 'X', 'T', '1');
        ddsHeader.dwPitchOrLinearSize = texture->texhdr.width * texture->texhdr.height / 2;	// 4bpp
        break;

      case DXGI_FORMAT_BC2_UNORM:
        ddsHeader.ddspf.dwFlags = DDS_FOURCC;
        ddsHeader.ddspf.dwFourCC = MAKEFOURCC('D', 'X', 'T', '3');
        ddsHeader.dwPitchOrLinearSize = texture->texhdr.width * texture->texhdr.height;	// 8bpp
        break;

      case DXGI_FORMAT_BC3_UNORM:
        ddsHeader.ddspf.dwFlags = DDS_FOURCC;
        ddsHeader.ddspf.dwFourCC = MAKEFOURCC('D', 'X', 'T', '5');
        ddsHeader.dwPitchOrLinearSize = texture->texhdr.width * texture->texhdr.height;	// 8bpp
        break;

      case DXGI_FORMAT_BC5_UNORM:
        ddsHeader.ddspf.dwFlags = DDS_FOURCC;
        if(m_UseATIFourCC)
          ddsHeader.ddspf.dwFourCC = MAKEFOURCC('A', 'T', 'I', '2');	// this is more correct but the only thing I have found that supports it is the nvidia photoshop plugin
        else
          ddsHeader.ddspf.dwFourCC = MAKEFOURCC('D', 'X', 'T', '5');

        ddsHeader.dwPitchOrLinearSize = texture->texhdr.width * texture->texhdr.height;	// 8bpp
        break;

      case DXGI_FORMAT_BC7_UNORM:
        // totally wrong but not worth writing out the DX10 header
        ddsHeader.ddspf.dwFlags = DDS_FOURCC;
        ddsHeader.ddspf.dwFourCC = MAKEFOURCC('B', 'C', '7', '\0');
        ddsHeader.dwPitchOrLinearSize = texture->texhdr.width * texture->texhdr.height;	// 8bpp
        break;

      case DXGI_FORMAT_B8G8R8A8_UNORM:
        ddsHeader.ddspf.dwFlags = DDS_RGBA;
        ddsHeader.ddspf.dwRGBBitCount = 32;
        ddsHeader.ddspf.dwRBitMask =	0x00FF0000;
        ddsHeader.ddspf.dwGBitMask =	0x0000FF00;
        ddsHeader.ddspf.dwBBitMask =	0x000000FF;
        ddsHeader.ddspf.dwABitMask =	0xFF000000;
        ddsHeader.dwPitchOrLinearSize = texture->texhdr.width * texture->texhdr.height * 4;	// 32bpp
        break;

      case DXGI_FORMAT_R8_UNORM:
        ddsHeader.ddspf.dwFlags = DDS_RGB;
        ddsHeader.ddspf.dwRGBBitCount = 8;
        ddsHeader.ddspf.dwRBitMask =	0xFF;
        ddsHeader.dwPitchOrLinearSize = texture->texhdr.width * texture->texhdr.height;	// 8bpp
        break;

      default:
        ok = false;
        break;
      }

      if(ok)
      {
        writeType<BSAULong>(outFile, DDS_MAGIC);
        writeType(outFile, ddsHeader);

        for(BSAULong j = 0; j < texture->texchunks.size(); ++j) {
          const DX10Chunk *chunk = &texture->texchunks[j];

          std::unique_ptr<BSAUChar[]> sourceBuffer(new BSAUChar[chunk->packedLen]);

          m_File.seekg(chunk->offset);
          m_File.read((char*)sourceBuffer.get(), chunk->packedLen);

          std::unique_ptr<BSAUChar[]> destinationBuffer(new BSAUChar[chunk->unpackedLen]);

          BSAULong bytesWritten = chunk->unpackedLen;
          int result = uncompress(destinationBuffer.get(), &bytesWritten, sourceBuffer.get(), chunk->packedLen);
          if ((result != Z_OK) || (bytesWritten != chunk->unpackedLen)) {
            return ERROR_INVALIDDATA;
          }

          outFile.write((char*)destinationBuffer.get(), chunk->unpackedLen);
        }
      }
    }
  }
  return ERROR_NONE;
}


void Archive::writeHeader(std::fstream &outfile, EType type, BSAULong fileVersion, BSAULong numFiles,
                          BSAHash nameTableOffset)
{
  // dummy code
  outfile.write("BTDX", 4);
  writeType<BSAULong>(outfile, fileVersion);
  outfile.write(typeToID(type), 4);
  writeType<BSAULong>(outfile, numFiles);
  writeType<BSAULong>(outfile, nameTableOffset);
}


inline bool fileExists(const std::string &name) {
  struct stat buffer;
  return stat(name.c_str(), &buffer) != -1;
}


} // namespace BA2
