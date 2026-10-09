// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <fstream>
#include <os.h>
#include "pipeline_cache_manager.h"

namespace skyline::gpu {
    struct PipelineCacheFileHeader {
        static constexpr u32 Magic{util::MakeMagic<u32>("PCHE")}; //!< The magic value used to identify a pipeline cache file
        static constexpr u32 Version{3}; //!< The version of the pipeline cache file format, MUST be incremented for any format changes

        u32 magic{Magic};
        u32 version{Version};
        u32 count{0}; //!< The total number of pipeline cache bundles in the file

        auto operator<=>(const PipelineCacheFileHeader &) const = default;

        /**
         * @brief Checks if the header is valid
         */
        bool IsValid() {
            return magic == Magic && version == Version;
        }
    };

    void PipelineCacheManager::Run() {
        std::ofstream stream{stagingPath, std::ios::binary | std::ios::trunc};
        PipelineCacheFileHeader header{};
        stream.write(reinterpret_cast<const char *>(&header), sizeof(header));
        stream.flush();

        // Shader creation can enqueue hundreds of bundles at once. Avoid
        // seeking to offset zero and rewriting the header for every shader,
        // which causes avoidable flash I/O and queue contention on Adreno.
        // Bound the uncommitted staging tail to at most 15 bundles.
        constexpr u32 HeaderBatchSize{16};
        u32 pendingHeaderUpdates{};

        const auto flushHeader{[&]() {
            if (!pendingHeaderUpdates)
                return;
            const auto savedPosition{stream.tellp()};
            stream.seekp(0, std::ios_base::beg);
            stream.write(reinterpret_cast<const char *>(&header), sizeof(header));
            stream.seekp(savedPosition);
            stream.flush();
            pendingHeaderUpdates = 0;
        }};

        while (true) {
            std::unique_lock lock(writeMutex);
            if (writeQueue.empty()) {
                lock.unlock();
                // Persist the count promptly when the shader queue goes idle.
                // If Android kills the app while busy, a short trailing cache
                // segment may be ignored safely on next launch.
                flushHeader();
                lock.lock();
            }

            writeCondition.wait(lock, [this] { return !writeQueue.empty(); });
            auto bundle{std::move(writeQueue.front())};
            writeQueue.pop();
            lock.unlock();

            bundle->Serialise(stream);
            ++header.count;
            if (++pendingHeaderUpdates >= HeaderBatchSize)
                flushHeader();
        }
    }

    bool PipelineCacheManager::ValidateHeader(std::ifstream &stream) {
        if (stream.fail())
            return false;

        PipelineCacheFileHeader header{};
        stream.read(reinterpret_cast<char *>(&header), sizeof(PipelineCacheFileHeader));
        // A truncated file may leave the default-initialised magic/version intact.
        // Accept the header only if the entire on-disk structure was read.
        return stream.gcount() == static_cast<std::streamsize>(sizeof(PipelineCacheFileHeader)) && header.IsValid();
    }

    void PipelineCacheManager::MergeStaging() {
        std::ifstream stagingStream{stagingPath, std::ios::binary};
        if (stagingStream.fail())
            return; // If the staging file doesn't exist then there's nothing to merge

        PipelineCacheFileHeader stagingHeader{};
        stagingStream.read(reinterpret_cast<char *>(&stagingHeader), sizeof(PipelineCacheFileHeader));
        if (stagingStream.gcount() != static_cast<std::streamsize>(sizeof(PipelineCacheFileHeader)) || !stagingHeader.IsValid()) {
            LOGW("Discarding invalid pipeline cache staging file");
            return;
        }

        std::fstream mainStream{mainPath, std::ios::binary | std::ios::in | std::ios::out};
        PipelineCacheFileHeader mainHeader{};
        mainStream.seekg(0, std::ios_base::beg);
        mainStream.read(reinterpret_cast<char *>(&mainHeader), sizeof(PipelineCacheFileHeader));
        if (mainStream.gcount() != static_cast<std::streamsize>(sizeof(PipelineCacheFileHeader)) || !mainHeader.IsValid()) {
            LOGW("Cannot merge staging data into an invalid pipeline cache");
            return;
        }

        // Append payload first, commit the new count only after the bytes
        // have been written successfully. The old code increased the count
        // before copying data, corrupting the cache on a short disk write.
        mainStream.seekp(0, std::ios::end);
        const auto originalEnd{mainStream.tellp()};
        if (originalEnd == std::streampos(-1)) {
            LOGW("Cannot determine graphics pipeline cache size; keeping staging");
            return;
        }
        if (stagingHeader.count > std::numeric_limits<u32>::max() - mainHeader.count) {
            LOGW("Pipeline cache count overflow; keeping staging for recovery");
            return;
        }

        mainStream << stagingStream.rdbuf();
        mainStream.flush();
        if (!mainStream || stagingStream.bad()) {
            LOGW("Pipeline cache append failed; restoring previous length");
            mainStream.close();
            std::error_code rollbackError;
            std::filesystem::resize_file(mainPath, static_cast<u64>(static_cast<std::streamoff>(originalEnd)), rollbackError);
            if (rollbackError)
                LOGW("Pipeline cache rollback failed: {}", rollbackError.message());
            return;
        }

        mainHeader.count += stagingHeader.count;
        mainStream.seekp(0, std::ios_base::beg);
        mainStream.write(reinterpret_cast<const char *>(&mainHeader), sizeof(PipelineCacheFileHeader));
        mainStream.flush();
        if (!mainStream) {
            LOGW("Pipeline cache header commit failed; keeping staging for recovery");
            return;
        }

        // Important: the next launch must not append the same staging bundles
        // again. A stale .staging file previously caused unbounded cache growth
        // and progressively longer cache replays.
        stagingStream.close();
        mainStream.close();
        std::error_code removeError;
        std::filesystem::remove(stagingPath, removeError);
        if (removeError)
            LOGW("Could not remove merged pipeline cache staging file: {}", removeError.message());
    }

    PipelineCacheManager::PipelineCacheManager(const DeviceState &state, const std::string &path)
        : stagingPath{path + ".staging"}, mainPath{path} {
        bool didExist{std::filesystem::exists(mainPath)};
        if (didExist) { // If the main file exists then we need to validate it
            std::ifstream mainStream{mainPath, std::ios::binary};
            if (!ValidateHeader(mainStream)) { // Force a recreation of the file if it's invalid
                LOGW("Discarding invalid pipeline cache main file");
                std::filesystem::remove(mainPath);
                didExist = false;
            }
        }

        if (!didExist) { // If the main file didn't exist we need to write the header
            std::filesystem::create_directories(std::filesystem::path{mainPath}.parent_path());
            std::ofstream mainStream{mainPath, std::ios::binary | std::ios::app};
            PipelineCacheFileHeader header{};
            mainStream.write(reinterpret_cast<const char *>(&header), sizeof(PipelineCacheFileHeader));
        }

        // Merge any staging changes into the main file before starting the writer thread
        MergeStaging();
        writerThread = std::thread(&PipelineCacheManager::Run, this);
    }

    void PipelineCacheManager::QueueWrite(std::unique_ptr<interconnect::PipelineStateBundle> bundle) {
        std::scoped_lock lock{writeMutex};
        writeQueue.emplace(std::move(bundle));
        writeCondition.notify_one();
    }

    std::pair<std::ifstream, u32> PipelineCacheManager::OpenReadStream() {
        auto mainStream{std::ifstream{mainPath, std::ios::binary}};
        if (mainStream.fail())
            throw exception("Pipeline cache main file missing at runtime!");

        PipelineCacheFileHeader header{};
        mainStream.read(reinterpret_cast<char *>(&header), sizeof(PipelineCacheFileHeader));
        if (mainStream.gcount() != static_cast<std::streamsize>(sizeof(PipelineCacheFileHeader)) || !header.IsValid())
            throw exception("Pipeline cache main file corrupted at runtime!");

        // Never trust a malformed count from disk: replaying an excessive
        // number of entries can turn a corrupt cache into a lengthy startup
        // stall or an out-of-bounds stream read. Every serialized bundle
        // occupies at least one byte after the fixed header.
        mainStream.seekg(0, std::ios::end);
        const auto endOffset{static_cast<std::streamoff>(mainStream.tellg())};
        constexpr auto headerSize{static_cast<std::streamoff>(sizeof(PipelineCacheFileHeader))};
        if (endOffset < headerSize ||
            static_cast<u64>(header.count) > static_cast<u64>(endOffset - headerSize))
            throw exception("Pipeline cache has an impossible pipeline count!");

        mainStream.seekg(sizeof(PipelineCacheFileHeader), std::ios::beg);
        if (!mainStream)
            throw exception("Failed to seek pipeline cache payload!");

        return {std::move(mainStream), header.count};
    }

    void PipelineCacheManager::InvalidateAllAfter(u64 offset) {
        std::filesystem::resize_file(mainPath, offset);
    }
}
