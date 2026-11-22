#include "pch.h"
#include "StreamBuffer.h"
#include "Streams.h"

std::unique_ptr<StreamBuffer> StreamBuffer::Open(std::wstring const& path, std::wstring_view name, DWORD& error) {
	wil::unique_hfile file(::CreateFile(Streams::StreamPath(path, name).c_str(), GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_RANDOM_ACCESS, nullptr));
	if (!file) {
		error = ::GetLastError();
		return nullptr;
	}
	LARGE_INTEGER size;
	if (!::GetFileSizeEx(file.get(), &size)) {
		error = ::GetLastError();
		return nullptr;
	}
	error = ERROR_SUCCESS;
	return std::unique_ptr<StreamBuffer>(new StreamBuffer(std::move(file), size.QuadPart));
}

bool StreamBuffer::LoadChunk(int64_t start) {
	if (start == m_ChunkStart)
		return true;
	m_Chunk.resize(ChunkSize);
	OVERLAPPED ov{};
	ov.Offset = (DWORD)start;
	ov.OffsetHigh = (DWORD)(start >> 32);
	DWORD read = 0;
	if (!::ReadFile(m_File.get(), m_Chunk.data(), ChunkSize, &read, &ov) && ::GetLastError() != ERROR_HANDLE_EOF) {
		m_ChunkStart = -1;
		return false;
	}
	m_Chunk.resize(read);
	m_ChunkStart = start;
	return true;
}

uint32_t StreamBuffer::GetData(int64_t offset, uint8_t* buffer, uint32_t count) {
	if (offset < 0 || offset >= m_Size)
		return 0;
	count = (uint32_t)(std::min<int64_t>)(count, m_Size - offset);
	uint32_t copied = 0;
	while (copied < count) {
		auto at = offset + copied;
		auto start = at - at % ChunkSize;
		if (!LoadChunk(start))
			break;
		auto inChunk = (size_t)(at - start);
		if (inChunk >= m_Chunk.size())
			break;	// the stream got shorter since it was opened
		auto n = (uint32_t)(std::min<size_t>)(count - copied, m_Chunk.size() - inChunk);
		::memcpy(buffer + copied, m_Chunk.data() + inChunk, n);
		copied += n;
	}
	return copied;
}
