#include <catch2/catch_test_macros.hpp>

#include <MeshData.hpp>
#include <RetireQueue.hpp>

#include <cmath>
#include <limits>
#include <memory>

using umbra::core::MeshData;
using umbra::core::RetireQueue;
using umbra::core::TextureData;

namespace
{
    MeshData Triangle()
    {
        MeshData mesh;
        mesh.vertices.resize(3);
        mesh.indices = {0, 1, 2};
        return mesh;
    }

    // Counts how many tracked objects have been destroyed.
    struct Tracked
    {
        std::shared_ptr<int> destroyed;
        Tracked(std::shared_ptr<int> counter) : destroyed(std::move(counter)) {}
        Tracked(Tracked &&) = default;
        // Like GPU wrappers: assigning over a live object releases it first.
        Tracked &operator=(Tracked &&other)
        {
            if (destroyed)
                ++*destroyed;
            destroyed = std::move(other.destroyed);
            return *this;
        }
        ~Tracked()
        {
            if (destroyed)
                ++*destroyed;
        }
    };
}

TEST_CASE("Valid mesh and texture pass validation")
{
    REQUIRE(umbra::core::ValidateMesh(Triangle()).empty());
    REQUIRE(umbra::core::ValidateTexture(TextureData{1, 1, {255, 255, 255, 255}}).empty());
}

TEST_CASE("Invalid meshes are rejected with a reason")
{
    MeshData empty;
    REQUIRE_FALSE(umbra::core::ValidateMesh(empty).empty());

    MeshData notTriangles = Triangle();
    notTriangles.indices.push_back(0);
    REQUIRE_FALSE(umbra::core::ValidateMesh(notTriangles).empty());

    MeshData outOfRange = Triangle();
    outOfRange.indices[2] = 3;
    REQUIRE_FALSE(umbra::core::ValidateMesh(outOfRange).empty());

    MeshData notFinite = Triangle();
    notFinite.vertices[0].position.x = std::numeric_limits<float>::quiet_NaN();
    REQUIRE_FALSE(umbra::core::ValidateMesh(notFinite).empty());
}

TEST_CASE("Invalid textures are rejected with a reason")
{
    REQUIRE_FALSE(umbra::core::ValidateTexture(TextureData{0, 1, {}}).empty());
    REQUIRE_FALSE(umbra::core::ValidateTexture(TextureData{2, 2, {1, 2, 3, 4}}).empty());
    REQUIRE_FALSE(umbra::core::ValidateTexture(TextureData{20000, 1, {}}).empty());
}

TEST_CASE("Retired objects live until their serial is completed")
{
    auto destroyed = std::make_shared<int>(0);
    RetireQueue<Tracked> queue;
    queue.Retire(Tracked{destroyed}, 5);
    queue.Retire(Tracked{destroyed}, 7);
    *destroyed = 0; // ignore moved-from temporaries

    REQUIRE(queue.Collect(4) == 0);
    REQUIRE(*destroyed == 0);
    REQUIRE(queue.Collect(5) == 1);
    REQUIRE(*destroyed == 1);
    REQUIRE(queue.Size() == 1);
    REQUIRE(queue.Collect(100) == 1);
    REQUIRE(*destroyed == 2);
    REQUIRE(queue.Size() == 0);
}

TEST_CASE("Flush destroys everything")
{
    auto destroyed = std::make_shared<int>(0);
    RetireQueue<Tracked> queue;
    queue.Retire(Tracked{destroyed}, 1);
    queue.Retire(Tracked{destroyed}, 2);
    *destroyed = 0;

    queue.Flush();
    REQUIRE(*destroyed == 2);
    REQUIRE(queue.Size() == 0);
}
