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
        stream.write(reinterpret_cast<const char *>(&header), sizeof(PipelineCacheFileHeader));

        while (true) {
            std::unique_lock lock(writeMutex);
            if (writeQueue.empty())
                stream.flush();

            writeCondition.wait(lock, [this] { return !writeQueue.empty(); });
            auto bundle{std::move(writeQueue.front())};
            writeQueue.pop();
            lock.unlock();

            bundle->Serialise(stream);

            header.count++;
            // Rewrite the header with the updated count
            auto savedPosition{stream.tellp()};
            stream.seekp(0, std::ios_base::beg);
            stream.write(reinterpret_cast<const char *>(&header), sizeof(PipelineCacheFileHeader));
            stream.seekp(savedPosition);
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
        if (!stagingStream)
            return;

        PipelineCacheFileHeader stagingHeader{};
        stagingStream.read(reinterpret_cast<char *>(&stagingHeader), sizeof(stagingHeader));
        if (stagingStream.gcount() != static_cast<std::streamsize>(sizeof(stagingHeader)) || !stagingHeader.IsValid()) {
            LOGW("Ignoring invalid staging pipeline cache");
            return;
        }

        std::ifstream mainStream{mainPath, std::ios::binary};
        PipelineCacheFileHeader mainHeader{};
        mainStream.read(reinterpret_cast<char *>(&mainHeader), sizeof(mainHeader));
        if (mainStream.gcount() != static_cast<std::streamsize>(sizeof(mainHeader)) || !mainHeader.IsValid()) {
            LOGW("Skipping staging merge: invalid main cache header");
            return;
        }

        // A failed append must NEVER publish the increased pipeline count.
        // Compose a new cache file and atomically publish it only after the
        // entire main + staging bodies are copied. Preserve the staging data
        // until replacement succeeds, so process termination is recoverable.
        if (stagingHeader.count == 0)
            return;
        if (stagingHeader.count > std::numeric_limits<u32>::max() - mainHeader.count) {
            LOGW("Skipping pipeline cache merge: count overflow");
            return;
        }
        const std::string temporaryPath{mainPath + ".merging"};
        std::ofstream output{temporaryPath, std::ios::binary | std::ios::trunc};
        if (!output) {
            LOGW("Cannot open temporary pipeline cache for merge");
            return;
        }

        const PipelineCacheFileHeader mergedHeader{
            .magic = PipelineCacheFileHeader::Magic,
            .version = PipelineCacheFileHeader::Version,
            .count = mainHeader.count + stagingHeader.count
        };
        output.write(reinterpret_cast<const char *>(&mergedHeader), sizeof(mergedHeader));
        output << mainStream.rdbuf();
        const bool mainCopied{!mainStream.bad() && !output.fail()};
        if (mainCopied)
            output << stagingStream.rdbuf();
        output.flush();
        const bool allCopied{mainCopied && !stagingStream.bad() && output.good()};
        output.close();
        mainStream.close();
        stagingStream.close();
        if (!allCopied) {
            LOGW("Could not merge pipeline caches; retaining staging data");
            std::error_code ec;
            std::filesystem::remove(temporaryPath, ec);
            return;
        }

        std::error_code ec;
        std::filesystem::rename(temporaryPath, mainPath, ec);
        if (ec) {
            LOGW("Pipeline cache atomic replace failed: {}", ec.message());
            std::filesystem::remove(temporaryPath, ec);
            return;
        }

        std::filesystem::remove(stagingPath, ec);
        if (ec)
            LOGW("Merged pipeline staging data could not be removed: {}", ec.message());
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
