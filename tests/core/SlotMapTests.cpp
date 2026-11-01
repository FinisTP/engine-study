#include <catch2/catch_test_macros.hpp>

#include <SlotMap.hpp>

#include <string>

using umbra::core::SlotMap;

TEST_CASE("SlotMap inserts, reads and removes values")
{
    SlotMap<std::string> map;
    const auto a = map.Insert("a");

    REQUIRE(a);
    REQUIRE(*map.Get(a) == "a");
    REQUIRE(map.Size() == 1);
    REQUIRE(map.Remove(a));
    REQUIRE(map.Get(a) == nullptr);
    REQUIRE(map.Size() == 0);
    REQUIRE_FALSE(map.Remove(a));
}

TEST_CASE("SlotMap reuses a slot without reviving the old handle")
{
    SlotMap<int> map;
    const auto first = map.Insert(1);
    map.Remove(first);
    const auto second = map.Insert(2);

    REQUIRE(second.index == first.index);
    REQUIRE(second.generation != first.generation);
    REQUIRE(map.Get(first) == nullptr);
    REQUIRE(*map.Get(second) == 2);
}

TEST_CASE("SlotMap rejects default and out-of-range handles")
{
    SlotMap<int> map;
    map.Insert(1);

    REQUIRE(map.Get({}) == nullptr);
    REQUIRE(map.Get({99, 1}) == nullptr);
}

TEST_CASE("SlotMap clear keeps old handles stale")
{
    SlotMap<int> map;
    const auto a = map.Insert(1);
    const auto b = map.Insert(2);
    map.Clear();
    const auto c = map.Insert(3);

    REQUIRE(map.Size() == 1);
    REQUIRE(map.Get(a) == nullptr);
    REQUIRE(map.Get(b) == nullptr);
    REQUIRE(*map.Get(c) == 3);
}

TEST_CASE("SlotMap visits live values only")
{
    SlotMap<int> map;
    map.Insert(1);
    const auto removed = map.Insert(2);
    map.Insert(3);
    map.Remove(removed);

    int sum = 0;
    map.ForEach([&](auto, int value) { sum += value; });
    REQUIRE(sum == 4);
}
