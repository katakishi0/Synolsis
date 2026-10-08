// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <vfs/os_filesystem.h>
#include <charconv>
#include <cctype>
#include "key_store.h"

namespace skyline::crypto {
    KeyStore::KeyStore(const std::string &rootPath) {
        vfs::OsFileSystem root(rootPath);
        if (root.FileExists("title.keys"))
            ReadPairs(root.OpenFile("title.keys"), &KeyStore::PopulateTitleKeys);
        if (root.FileExists("prod.keys"))
            ReadPairs(root.OpenFile("prod.keys"), &KeyStore::PopulateKeys);
    }

    void KeyStore::ReadPairs(const std::shared_ptr<vfs::Backing> &backing, ReadPairsCallback callback) {
        std::vector<char> fileContent(backing->size);
        if (fileContent.empty())
            return;
        backing->Read(span(fileContent));

        // Accept LF/CRLF, whitespace, comments and keys without a final newline.
        const auto trim{[](std::string_view &token) {
            while (!token.empty() && std::isspace(static_cast<unsigned char>(token.front())))
                token.remove_prefix(1);
            while (!token.empty() && std::isspace(static_cast<unsigned char>(token.back())))
                token.remove_suffix(1);
        }};
        std::string_view content{fileContent.data(), fileContent.size()};
        size_t start{};
        while (start < content.size()) {
            const auto end{content.find('\n', start)};
            const auto length{end == std::string_view::npos ? content.size() - start : end - start};
            auto line{content.substr(start, length)};
            trim(line);
            if (!line.empty() && line.front() != ';' && line.front() != '#') {
                const auto delimiter{line.find('=')};
                if (delimiter == std::string_view::npos)
                    throw exception("Invalid key file");
                auto key{line.substr(0, delimiter)};
                auto value{line.substr(delimiter + 1)};
                if (const auto comment{value.find(';')}; comment != std::string_view::npos)
                    value = value.substr(0, comment);
                trim(key);
                trim(value);
                if (key.empty() || value.empty())
                    throw exception("Invalid key file");
                (this->*callback)(key, value);
            }
            if (end == std::string_view::npos)
                break;
            start = end + 1;
        }
    }

    void KeyStore::PopulateTitleKeys(std::string_view keyName, std::string_view value) {
        Key128 key{util::HexStringToArray<16>(keyName)};
        Key128 valueArray{util::HexStringToArray<16>(value)};
        titleKeys.emplace(key, valueArray);
    }

    void KeyStore::PopulateTitleKey(Key128 keyName, Key128 value) {
        if (!titleKeys.contains(keyName))
            titleKeys.emplace(keyName, value);
    }

    void KeyStore::PopulateKeys(std::string_view keyName, std::string_view value) {
        {
            auto it{key256Names.find(keyName)};
            if (it != key256Names.end()) {
                it->second = headerKey = util::HexStringToArray<32>(value);
                return;
            }
        }

        if (keyName.size() > 2) {
            auto it{indexedKey128Names.find(keyName.substr(0, keyName.size() - 2))};
            if (it != indexedKey128Names.end()) {
                // Do not index outside the reserved key generations.
                const auto suffix{keyName.substr(it->first.size())};
                unsigned int index{};
                const auto parsed{std::from_chars(suffix.data(), suffix.data() + suffix.size(), index, 16)};
                if (parsed.ec != std::errc{} || parsed.ptr != suffix.data() + suffix.size() || index >= it->second.size())
                    return;
                it->second[index] = util::HexStringToArray<16>(value);
            }
        }
    }
}
