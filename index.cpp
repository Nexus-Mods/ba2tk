#include "nbind/noconflict.h"
#include "ba2tk/src/ba2archive.h"
#include <vector>
#include <nan.h>


const char *convertErrorCode(BA2::EErrorCode code) {
  switch (code) {
    case BA2::ERROR_ACCESSFAILED: return "access failed";
    case BA2::ERROR_CANCELED: return "canceled";
    case BA2::ERROR_FILENOTFOUND: return "file not found";
    case BA2::ERROR_INVALIDDATA: return "invalid data";
    case BA2::ERROR_INVALIDHASHES: return "invalid hashes";
    case BA2::ERROR_SOURCEFILEMISSING: return "source file missing";
    case BA2::ERROR_ZLIBINITFAILED: return "zlib init failed";
    case BA2::ERROR_NONE: return nullptr;
    default: return "unknown";
  }
}

class ExtractWorker : public Nan::AsyncWorker {
public:
  ExtractWorker(std::shared_ptr<BA2::Archive> archive,
             BA2::File::Ptr file,
             const char *outputrDirectory,
             Nan::Callback *appCallback)
    : Nan::AsyncWorker(appCallback)
    , m_Archive(archive)
    , m_File(file)
    , m_OutputDirectory(outputrDirectory)
  {}

  void Execute() {
    BA2::EErrorCode code;
    if (m_File.get() != nullptr) {
      code = m_Archive->extract(m_File, m_OutputDirectory.c_str());
    }
    else {
      code = m_Archive->extractAll(m_OutputDirectory.c_str(),
        [](int, std::string) { return true; });
    }
    if (code != BA2::ERROR_NONE) {
      SetErrorMessage(convertErrorCode(code));
    }
  }

  void HandleOKCallback() {
    Nan::HandleScope scope;

    v8::Local<v8::Value> argv[] = {
      Nan::Null()
    };

    callback->Call(1, argv);
  }
private:
  std::shared_ptr<BA2::Archive> m_Archive;
  std::shared_ptr<BA2::File> m_File;
  std::string m_OutputDirectory;
};

class BSAFile {
public:
  BSAFile(std::shared_ptr<BSA::File> file)
    : m_File(file)
  {}

  BSA::File::Ptr getWrappee() const { return m_File; }

  std::string getName() const { return m_File->getName(); }
  std::string getFilePath() const { return m_File->getFilePath(); }
  unsigned long getFileSize() const { return m_File->getFileSize(); }

private:
  BSA::File::Ptr m_File;
};

class BSAFolder {
public:
  BSAFolder(std::shared_ptr<BSA::Folder> folder)
    : m_Folder(folder)
  {}

  std::string getName() const { return m_Folder->getName(); }
  std::string getFullPath() const { return m_Folder->getFullPath(); }
  unsigned int getNumSubFolders() const { return m_Folder->getNumSubFolders(); }
  BSAFolder getSubFolder(unsigned int index) const { return BSAFolder(m_Folder->getSubFolder(index)); }
  unsigned int getNumFiles() const { return m_Folder->getNumFiles(); }
  unsigned int countFiles() const { return m_Folder->countFiles(); }
  const BSAFile getFile(unsigned int index) const { return BSAFile(m_Folder->getFile(index)); }
  void addFile(const BSAFile &file) { m_Folder->addFile(file.getWrappee()); }
  BSAFolder addFolder(const std::string &folderName) { return BSAFolder(m_Folder->addFolder(folderName)); }

private:
  std::shared_ptr<BSA::Folder> m_Folder;
};

class BSArchive {
public:
  BSArchive(const char *fileName, bool testHashes)
    : m_Wrapped(new BSA::Archive())
  {
    read(fileName, testHashes);
  }

  BSArchive(const BSArchive &ref)
    : m_Wrapped(ref.m_Wrapped)
  {
  }

  ~BSArchive() {
  }

  BSAFolder getRoot() {
    return BSAFolder(m_Wrapped->getRoot());
  }

  const char *getType() const {
    switch (m_Wrapped->getType()) {
      case BSA::TYPE_OBLIVION: return "oblivion";
      case BSA::TYPE_SKYRIM:   return "skyrim";
      // fallout 3 and fallout nv use the same type as skyrim
      default: return nullptr;
    }
  }

  void read(const char *fileName, bool testHashes) {
    BSA::EErrorCode err = m_Wrapped->read(fileName, testHashes);
    if (err != BA2::ERROR_NONE) {
      throw std::runtime_error(convertErrorCode(err));
    }
  }

private:
  std::shared_ptr<BA2::Archive> m_Wrapped;
};

class LoadWorker : public Nan::AsyncWorker {
public:
  LoadWorker(const char *fileName, bool testHashes, Nan::Callback *appCallback)
    : Nan::AsyncWorker(appCallback)
    , m_OutputDirectory(fileName)
    , m_TestHashes(testHashes)
  {
  }

  void Execute() {
    try {
      m_Result = new BSArchive(m_OutputDirectory.c_str(), m_TestHashes);
    }
    catch (const std::exception &e) {
      SetErrorMessage(e.what());
    }
  }

  void HandleOKCallback() {
    Nan::HandleScope scope;

    v8::Local<v8::Value> argv[] = {
      Nan::Null()
      , nbind::convertToWire(*m_Result)
    };

    callback->Call(2, argv);
    delete m_Result;
  }
private:
  BA2rchive *m_Result;
  std::string m_OutputDirectory;
  bool m_TestHashes;
};


void loadBA2(const char *fileName, bool testHashes, nbind::cbFunction &callback) {
  Nan::AsyncQueueWorker(
    new LoadWorker(
      fileName, testHashes,
      new Nan::Callback(callback.getJsFunction())
  ));
}


NBIND_CLASS(BA2rchive) {
  NBIND_CONSTRUCT<const char*, bool>();
  NBIND_GETTER(getType);
  NBIND_METHOD(extract);
}

NBIND_FUNCTION(loadBA2);
