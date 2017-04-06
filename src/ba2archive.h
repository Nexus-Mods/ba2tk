/*
Nexus Mod Manager 2 BA2 handling

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 3 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/


#ifndef BA2_ARCHIVE_H
#define BA2_ARCHIVE_H


#include "errorcodes.h"
#include "ba2types.h"
#include "semaphore.h"
#include <vector>
#include <queue>
#include <functional>
#include <memory>
#include <mutex>


namespace BA2 {

template<typename T>
struct array_deleter
{
  void operator ()(const T *p)
  { 
    delete[] p; 
  }
};
  
class File;

/**
 * @brief top level structure to represent a bsa file
 */
class Archive {

public:

  typedef std::pair<std::shared_ptr<unsigned char>, BSAULong> DataBuffer;


public:
  /**
   * constructor
   */
  Archive();
  ~Archive();
  /**
   * read the archive from file
   * @param fileName name of the file to read from
   * @return ERROR_NONE on success or an error code
   */
  EErrorCode read(const char *fileName);
  /**
   * write the archive to disc
   * @param fileName name of the file to write to
   * @return ERROR_NONE on success or an error code
   */
  EErrorCode write(const char *fileName);
  /**
   * @brief close the archive
   */
  void close();
  /**
   * change the archive type
   * @param type new archive type
   */
  void setType(EType type) { m_Type = type; }
  /**
   * @return type of the archive
   */
  EType getType() const { return m_Type; }
  /**
   * extract a file from the archive
   * @param outputDirectory name of the directory to extract to.
   *                        may be absolute or relative
   * @return ERROR_NONE on success or an error code
   */
  EErrorCode extract(const char *outputDirectory) const;

  /**
   * extract all files. this is potentially faster than iterating over all files and
   * extracting each
   * @param outputDirectory name of the directory to extract to.
   *                        may be absolute or relative
   * @param progress callback function called on progress
   * @param overwrite if true (default) files are overwritten if they exist
   * @return ERROR_NONE on success or an error code
   */
  EErrorCode extractAll(const char *outputDirectory,
                        const std::function<bool (int value, std::string fileName)> &progress,
                        bool overwrite = true);


private:

  struct Header {
    char fileIdentifier[4];
    BSAULong version;
    char bsaType[4];
    BSAULong fileCount;
    BSAHash offsetNameTable;
  };

	struct FileEntry
	{
		BSAULong	unk00;			// 00 - name hash?
		char	ext[4];			// 04 - extension
		BSAULong	unk08;			// 08 - directory hash?
		BSAULong	unk0C;			// 0C - flags? 00100100
		BSAHash	offset;			// 10 - relative to start of file
		BSAULong	packedLen;		// 18 - packed length (zlib)
		BSAULong	unpackedLen;	// 1C - unpacked length
		BSAULong	unk20;			// 20 - BAADF00D
	};

  struct FileEntry_DX10
	{
		BSAULong	nameHash;		// 00
		char	ext[4];			// 04
		BSAULong	dirHash;		// 08
		UInt8	unk0C;			// 0C
		UInt8	numChunks;		// 0D
		UInt16	chunkHdrLen;	// 0E - size of one chunk header
		UInt16	height;			// 10
		UInt16	width;			// 12
		UInt8	numMips;		// 14
		UInt8	format;			// 15 - DXGI_FORMAT
		UInt16	unk16;			// 16 - 0800
	};

  struct DX10Chunk
	{
		UInt64	offset;			// 00
		UInt32	packedLen;		// 08
		UInt32	unpackedLen;	// 0C
		UInt16	startMip;		// 10
		UInt16	endMip;			// 12
		UInt32	unk14;			// 14 - BAADFOOD
	};

	struct Texture
	{
		FileEntry_DX10 texhdr;
		std::vector <DX10Chunk>	texchunks;
	};

  struct FileInfo {
    File::Ptr file;
    DataBuffer data;
  };


private:

  static Header readHeader(std::fstream &infile);

  static EType typeFromID(char typeID);

	bool readGeneral();
	bool readDX10();
	bool readNametable();

  void Extract(const char *destination);
  void extractGeneral(const char *destination);
	void extractDX10(const char *destination);

  void UseATIFourCC() { m_useATIFourCC = false; }

  char typeToID(EType type);

  BSAULong countFiles() const;

  BSAULong countCharacters(const std::vector<std::string> &list) const;
  BSAULong determineFileFlags(const std::vector<std::string> &fileList) const;


private:

  mutable std::fstream m_File;

	std::vector <FileEntry> m_Files;
	std::vector <Texture> m_Textures;
	std::vector <std::string> m_TableNames;

  EType m_Type;
  Header m_Header;

  bool m_useATIFourCC;

  std::mutex m_ReaderMutex;
  std::mutex m_ExtractMutex;

};

} // namespace BA2

#endif // BA2_ARCHIVE_H

