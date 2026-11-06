#pragma once

#include <SlotMap.hpp>

namespace umbra::core
{
    // Engine-neutral handles. Gameplay and world code can store these without
    // seeing any graphics or physics library type; the owning module resolves them.
    struct MeshTag;
    struct TextureTag;
    struct BodyTag;
    struct CharacterTag;
    using MeshHandle = Handle<MeshTag>;
    using TextureHandle = Handle<TextureTag>;
    using BodyHandle = Handle<BodyTag>;
    using CharacterHandle = Handle<CharacterTag>;
}
