#pragma once

#include <cstddef>
#include <filesystem>
#include <span>
#include <string>

namespace umbra::assets
{
    // Phones (P11, apps/umbra/Mobile.md): the game's files travel inside the app package as
    // game.pack (the files back to back) + game.pack.json (where each one is, and a hash). On first
    // start, and after an update, they are written to the app's own folder.

    // The hash in a pack index ("" if it isn't one).
    [[nodiscard]] std::string PackHash(const std::string &index);
    // The hash of the pack last unpacked into `target` ("" if none).
    [[nodiscard]] std::string InstalledPackHash(const std::filesystem::path &target);
    // Writes every file of the pack under `target` (only safe relative paths), then records the
    // hash, so an interrupted unpack is redone next start. False with a reason on failure.
    bool UnpackGame(const std::string &index, std::span<const std::byte> bin, const std::filesystem::path &target, std::string &error);
}
