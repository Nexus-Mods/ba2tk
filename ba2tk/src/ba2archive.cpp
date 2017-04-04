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
#include <IFileStream.h>
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


EType Archive::typeFromID(char *typeID)
{
  switch (typeID) {
    case "GNRL": return TYPE_GENERAL;
    case "DX10": return TYPE_DX10;
    default: throw data_invalid_exception(makeString("invalid type %d", typeID));
  }
}


char Archive::typeToID(EType type)
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
  result.bsatype  = typeFromID(readType<char>(infile));
  result.fileCount        = readType<BSAULong>(infile);
  result.offsetNameTable  = readType<BSAHash>(infile); 

  return result;
}


EErrorCode Archive::read(const char *fileName)
{
  m_File.open(fileName, fstream::in | fstream::binary);
  if (!m_File.is_open()) {
    return ERROR_FILENOTFOUND;
  }
  m_File.exceptions(std::ios_base::badbit);
  try {
    try {
      m_Header = readHeader(m_File);
    } catch (const data_invalid_exception &e) {
      throw data_invalid_exception(makeString("%s (filename: %s)", e.what(), fileName));
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

    BSAHash headerEnd = m_File->GetOffset();
    
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
	if(m_Header.fileCount)
		m_File->ReadBuf(&m_Files[0], sizeof(FileEntry) * m_Header.fileCount);

	return true;
}


bool Archive::readDX10()
{
	m_Textures.resize(m_Header.fileCount);

	for(UInt32 i = 0; i < m_Textures.size(); i++)
	{
		Texture *texture = &m_Textures[i];

		m_File->ReadBuf(&texture->texhdr, sizeof(texture->texhdr));

		texture->texchunks.resize(texture->texhdr.numChunks);
		if(texture->texhdr.numChunks)
			m_File->ReadBuf(&texture->texchunks[0], sizeof(DX10Chunk) * texture->texhdr.numChunks);
	}

	return true;
}


bool Archive::readNametable()
{
	m_File->SetOffset(m_Header.nameTableOffset);
	char *buffer = new char[0x10000];
	UInt32 index = 0;

	while(m_File->GetRemain() >= 2)
	{
		UInt32 length = m_File->Read16();

		m_File->ReadBuf(buffer, length);
		buffer[length] = 0;

		m_TableNames.push_back(buffer);

		index++;
	}

	delete [] buffer;
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

EErrorCode Archive::Extract(const char *destination)
{
	if(m_hdr.IsGeneral())
		extractGeneral(destination);
	else if(m_hdr.IsDX10())
		extractDX10(destination);
	else
		return ERROR_INVALIDDATA;

  return ERROR_NONE;
}


void Archive::extractGeneral(const char *destination)
{
	ASSERT(m_Files.size() == m_TableNames.size());

	for(BSAULong i = 0; i < m_Files.size(); i++)
	{
		FileEntry *file = &m_Files[i];

		std::string destinationPath = destination;
		destinationPath += "\\";
		destinationPath += m_TableNames[i];

		IFileStream::MakeAllDirs(destinationPath.c_str());

		IFileStream fileStream;
		if(fileStream.Create(destinationPath.c_str()))
		{
			m_File->SetOffset(file->offset);

			if(file->packedLen && (file->unpackedLen != file->packedLen))
			{
				BSAULong unpackedLen = file->unpackedLen;
				if(!unpackedLen)
					unpackedLen = file->unk20;	// what

				BSAUChar *sourceBuffer = new BSAUChar[file->packedLen];

				m_File->ReadBuf(sourceBuffer, file->packedLen);

				BSAUChar *destinationBuffer = new BSAUChar[unpackedLen];

				BSAULong bytesWritten = unpackedLen;
				int result = uncompress(destinationBuffer, &bytesWritten, sourceBuffer, file->packedLen);
				ASSERT(result == Z_OK);
				ASSERT(bytesWritten == unpackedLen);

				fileStream.WriteBuf(destinationBuffer, unpackedLen);

				delete [] destinationBuffer;
				delete [] sourceBuffer;
			}
			else
			{
				IDataStream::CopySubStreams(&fileStream, m_File, file->unpackedLen);
			}
		}
	}
}

void Archive::extractDX10(const char *destination)
{
	for(BSAULong i = 0; i < m_Textures.size(); i++)
	{
		Texture *texture = &m_Textures[i];

		std::string destinationPath = destination;
		destinationPath += "\\";
		destinationPath += m_TableNames[i];

		IFileStream::MakeAllDirs(destinationPath.c_str());

		IFileStream fileStream;
		if(fileStream.Create(destinationPath.c_str()))
		{
			DDS_HEADER ddsHeader = { 0 };

			ddsHeader.dwSize = sizeof(ddsHeader);
			ddsHeader.dwHeaderFlags = DDS_HEADER_FLAGS_TEXTURE | DDS_HEADER_FLAGS_LINEARSIZE | DDS_HEADER_FLAGS_MIPMAP;
			ddsHeader.dwHeight = texture->hdr.height;
			ddsHeader.dwWidth = texture->hdr.width;
			ddsHeader.dwMipMapCount = texture->hdr.numMips;
			ddsHeader.ddspf.dwSize = sizeof(DDS_PIXELFORMAT);
			ddsHeader.dwSurfaceFlags = DDS_SURFACE_FLAGS_TEXTURE | DDS_SURFACE_FLAGS_MIPMAP;

			bool ok = true;

			switch(texture->hdr.format)
			{
			case DXGI_FORMAT_BC1_UNORM:
				ddsHeader.ddspf.dwFlags = DDS_FOURCC;
				ddsHeader.ddspf.dwFourCC = MAKEFOURCC('D', 'X', 'T', '1');
				ddsHeader.dwPitchOrLinearSize = texture->hdr.width * texture->hdr.height / 2;	// 4bpp
				break;

			case DXGI_FORMAT_BC2_UNORM:
				ddsHeader.ddspf.dwFlags = DDS_FOURCC;
				ddsHeader.ddspf.dwFourCC = MAKEFOURCC('D', 'X', 'T', '3');
				ddsHeader.dwPitchOrLinearSize = texture->hdr.width * texture->hdr.height;	// 8bpp
				break;

			case DXGI_FORMAT_BC3_UNORM:
				ddsHeader.ddspf.dwFlags = DDS_FOURCC;
				ddsHeader.ddspf.dwFourCC = MAKEFOURCC('D', 'X', 'T', '5');
				ddsHeader.dwPitchOrLinearSize = texture->hdr.width * texture->hdr.height;	// 8bpp
				break;

			case DXGI_FORMAT_BC5_UNORM:
				ddsHeader.ddspf.dwFlags = DDS_FOURCC;
				if(m_useATIFourCC)
					ddsHeader.ddspf.dwFourCC = MAKEFOURCC('A', 'T', 'I', '2');	// this is more correct but the only thing I have found that supports it is the nvidia photoshop plugin
				else
					ddsHeader.ddspf.dwFourCC = MAKEFOURCC('D', 'X', 'T', '5');

				ddsHeader.dwPitchOrLinearSize = texture->hdr.width * texture->hdr.height;	// 8bpp
				break;

			case DXGI_FORMAT_BC7_UNORM:
				// totally wrong but not worth writing out the DX10 header
				ddsHeader.ddspf.dwFlags = DDS_FOURCC;
				ddsHeader.ddspf.dwFourCC = MAKEFOURCC('B', 'C', '7', '\0');
				ddsHeader.dwPitchOrLinearSize = texture->hdr.width * texture->hdr.height;	// 8bpp
				break;

			case DXGI_FORMAT_B8G8R8A8_UNORM:
				ddsHeader.ddspf.dwFlags = DDS_RGBA;
				ddsHeader.ddspf.dwRGBBitCount = 32;
				ddsHeader.ddspf.dwRBitMask =	0x00FF0000;
				ddsHeader.ddspf.dwGBitMask =	0x0000FF00;
				ddsHeader.ddspf.dwBBitMask =	0x000000FF;
				ddsHeader.ddspf.dwABitMask =	0xFF000000;
				ddsHeader.dwPitchOrLinearSize = texture->hdr.width * texture->hdr.height * 4;	// 32bpp
				break;

			case DXGI_FORMAT_R8_UNORM:
				ddsHeader.ddspf.dwFlags = DDS_RGB;
				ddsHeader.ddspf.dwRGBBitCount = 8;
				ddsHeader.ddspf.dwRBitMask =	0xFF;
				ddsHeader.dwPitchOrLinearSize = texture->hdr.width * texture->hdr.height;	// 8bpp
				break;

			default:
				ok = false;
				break;
			}

			if(ok)
			{
				fileStream.Write32(DDS_MAGIC);	// 'DDS '
				fileStream.WriteBuf(&ddsHeader, sizeof(ddsHeader));

				for(BSAULong j = 0; j < texture->chunks.size(); j++)
				{
					DX10Chunk *chunk = &texture->chunks[j];

					BSAUChar *sourceBuffer = new BSAUChar[chunk->packedLen];

					m_File->SetOffset(chunk->offset);
					m_File->ReadBuf(sourceBuffer, chunk->packedLen);

					BSAUChar *destinationBuffer = new BSAUChar[chunk->unpackedLen];

					BSAULong bytesWritten = chunk->unpackedLen;
					int result = uncompress(destinationBuffer, &bytesWritten, sourceBuffer, chunk->packedLen);
					ASSERT(result == Z_OK);
					ASSERT(bytesWritten == chunk->unpackedLen);

					fileStream.WriteBuf(destinationBuffer, chunk->unpackedLen);

					delete [] destinationBuffer;
					delete [] sourceBuffer;
				}
			}
		}
	}
}


void Archive::writeHeader(std::fstream &outfile, BSAULong fileVersion, BSAULong numFiles,
                          BSAHash nameTableOffset)
{
  // dummy code
  outfile.write("BTDX", 4);
  writeType<BSAULong>(outfile, fileVersion);
  writeType<BSAUChar>(outfile, typeToID(m_Type));
  writeType<BSAULong>(outfile, numFiles);
  writeType<BSAULong>(outfile, nameTableOffset);
}


inline bool fileExists(const std::string &name) {
  struct stat buffer;
  return stat(name.c_str(), &buffer) != -1;
}


} // namespace BA2
