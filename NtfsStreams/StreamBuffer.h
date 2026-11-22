#pragma once

#include <HexControl/BufferManager.h>

//
// The hex control's data: a stream read on demand (one cached chunk), so a large stream isn't read into memory. Read-only:
// the size is the stream's when it was opened, and the editing calls fail.
//
class StreamBuffer : public IBufferManager {
public:
	// the stream opened for reading; null with error set if it can't be
	static std::unique_ptr<StreamBuffer> Open(std::wstring const& path, std::wstring_view name, DWORD& error);

	uint32_t GetData(int64_t offset, uint8_t* buffer, uint32_t count) override;
	bool Insert(int64_t, const uint8_t*, uint32_t) override {
		return false;
	}
	bool Delete(int64_t, size_t) override {
		return false;
	}
	bool SetData(int64_t, const uint8_t*, uint32_t) override {
		return false;
	}
	bool SetData(int64_t, uint8_t, uint32_t) override {
		return false;
	}
	int64_t GetSize() const override {
		return m_Size;
	}
	uint8_t* GetRawData(int64_t) override {
		return nullptr;
	}
	bool IsReadOnly() const override {
		return true;
	}

	static constexpr uint32_t ChunkSize = 1 << 16;

private:
	StreamBuffer(wil::unique_hfile file, int64_t size) : m_File(std::move(file)), m_Size(size) {}
	// the chunk that starts at the offset (a multiple of ChunkSize) read into m_Chunk
	bool LoadChunk(int64_t start);

	wil::unique_hfile m_File;
	int64_t m_Size;
	int64_t m_ChunkStart{ -1 };
	std::vector<uint8_t> m_Chunk;
};
