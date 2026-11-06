#include <Physics.hpp>

#include <SlotMap.hpp>

// Jolt.h must come first.
#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

#include <algorithm>
#include <cmath>
#include <mutex>

namespace umbra::physics
{
    namespace
    {
        // Two object layers: static level geometry never needs to test against itself.
        constexpr JPH::ObjectLayer staticLayer = 0;
        constexpr JPH::ObjectLayer movingLayer = 1;
        constexpr JPH::BroadPhaseLayer staticBroadPhase{0};
        constexpr JPH::BroadPhaseLayer movingBroadPhase{1};

        class BroadPhaseLayers final : public JPH::BroadPhaseLayerInterface
        {
        public:
            JPH::uint GetNumBroadPhaseLayers() const override { return 2; }
            JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override
            {
                return layer == staticLayer ? staticBroadPhase : movingBroadPhase;
            }
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
            const char *GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override
            {
                return layer == staticBroadPhase ? "Static" : "Moving";
            }
#endif
        };

        class ObjectVsBroadPhase final : public JPH::ObjectVsBroadPhaseLayerFilter
        {
        public:
            bool ShouldCollide(JPH::ObjectLayer layer, JPH::BroadPhaseLayer broadPhase) const override
            {
                return layer == movingLayer || broadPhase == movingBroadPhase;
            }
        };

        class ObjectPairs final : public JPH::ObjectLayerPairFilter
        {
        public:
            bool ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const override
            {
                return a == movingLayer || b == movingLayer;
            }
        };

        // Jolt has process-wide state (allocator, factory, type registry):
        // set it up with the first PhysicsWorld and tear it down with the last.
        std::mutex joltMutex;
        int joltUsers = 0;

        struct JoltLifetime final
        {
            JoltLifetime()
            {
                std::lock_guard lock(joltMutex);
                if (joltUsers++ == 0)
                {
                    JPH::RegisterDefaultAllocator();
                    JPH::Factory::sInstance = new JPH::Factory();
                    JPH::RegisterTypes();
                }
            }
            ~JoltLifetime()
            {
                std::lock_guard lock(joltMutex);
                if (--joltUsers == 0)
                {
                    JPH::UnregisterTypes();
                    delete JPH::Factory::sInstance;
                    JPH::Factory::sInstance = nullptr;
                }
            }
            JoltLifetime(const JoltLifetime &) = delete;
            JoltLifetime &operator=(const JoltLifetime &) = delete;
        };

        JPH::Vec3 ToJolt(const glm::vec3 &v) { return {v.x, v.y, v.z}; }
        JPH::Quat ToJolt(const glm::quat &q) { return {q.x, q.y, q.z, q.w}; }
        glm::vec3 FromJolt(const JPH::Vec3 &v) { return {v.GetX(), v.GetY(), v.GetZ()}; }
        glm::quat FromJolt(const JPH::Quat &q) { return {q.GetW(), q.GetX(), q.GetY(), q.GetZ()}; }

        bool Positive(float value) { return std::isfinite(value) && value > 0.0F; }

        // Returns nullptr for invalid sizes.
        JPH::ShapeRefC MakeShape(const Shape &shape)
        {
            switch (shape.kind)
            {
            case ShapeKind::Box:
            {
                const glm::vec3 &h = shape.halfExtents;
                if (!Positive(h.x) || !Positive(h.y) || !Positive(h.z))
                    return nullptr;
                // Rounded edges must fit inside the box.
                const float convexRadius = std::min(0.05F, 0.5F * std::min({h.x, h.y, h.z}));
                return new JPH::BoxShape(ToJolt(h), convexRadius);
            }
            case ShapeKind::Sphere:
                return Positive(shape.radius) ? new JPH::SphereShape(shape.radius) : nullptr;
            case ShapeKind::Capsule:
                return Positive(shape.radius) && Positive(shape.halfHeight) ? new JPH::CapsuleShape(shape.halfHeight, shape.radius) : nullptr;
            }
            return nullptr;
        }

        // Body user data stores our handle so ray hits can be mapped back.
        JPH::uint64 Pack(BodyHandle h) { return (JPH::uint64{h.generation} << 32) | h.index; }
        BodyHandle Unpack(JPH::uint64 v) { return {static_cast<std::uint32_t>(v & 0xFFFFFFFFu), static_cast<std::uint32_t>(v >> 32)}; }
    }

    struct PhysicsWorld::Impl final
    {
        struct Body final
        {
            JPH::BodyID id;
            Shape shape;
            Motion motion = Motion::Static;
        };

        struct Character final
        {
            JPH::Ref<JPH::CharacterVirtual> jolt;
            CharacterDesc desc;
        };

        // Order matters: Jolt's global state must exist before and outlive everything below.
        JoltLifetime jolt;
        BroadPhaseLayers broadPhaseLayers;
        ObjectVsBroadPhase objectVsBroadPhase;
        ObjectPairs objectPairs;
        JPH::TempAllocatorImpl temp{10 * 1024 * 1024};
        JPH::JobSystemSingleThreaded jobs{JPH::cMaxPhysicsJobs};
        JPH::PhysicsSystem system;
        core::SlotMap<Body, core::BodyTag> bodies;
        core::SlotMap<Character, core::CharacterTag> characters;
        bool bodiesAdded = false;

        Impl()
        {
            constexpr JPH::uint maxBodies = 10240, maxBodyPairs = 65536, maxContacts = 10240;
            system.Init(maxBodies, 0, maxBodyPairs, maxContacts, broadPhaseLayers, objectVsBroadPhase, objectPairs);
        }

        ~Impl()
        {
            // Bodies must leave the system before it is destroyed.
            characters.Clear();
            JPH::BodyInterface &bi = system.GetBodyInterface();
            bodies.ForEach([&](auto, Body &body) {
                bi.RemoveBody(body.id);
                bi.DestroyBody(body.id);
            });
        }
    };

    PhysicsWorld::PhysicsWorld() : impl_(std::make_unique<Impl>())
    {
        impl_->system.SetGravity(ToJolt(gravity));
    }

    PhysicsWorld::~PhysicsWorld() = default;

    BodyHandle PhysicsWorld::CreateBody(const BodyDesc &desc)
    {
        const JPH::ShapeRefC shape = MakeShape(desc.shape);
        if (shape == nullptr)
            return {};

        // 1. Static bodies live in the static layer; everything that moves in the moving layer.
        const JPH::EMotionType motion = desc.motion == Motion::Static      ? JPH::EMotionType::Static
                                        : desc.motion == Motion::Kinematic ? JPH::EMotionType::Kinematic
                                                                           : JPH::EMotionType::Dynamic;
        JPH::BodyCreationSettings settings(shape, JPH::RVec3(ToJolt(desc.position)), ToJolt(glm::normalize(desc.rotation)), motion,
                                           desc.motion == Motion::Static ? staticLayer : movingLayer);
        settings.mFriction = desc.friction;
        settings.mRestitution = desc.restitution;

        // 2. Add it, then remember it under a generational handle.
        JPH::BodyInterface &bi = impl_->system.GetBodyInterface();
        const JPH::BodyID id = bi.CreateAndAddBody(settings, desc.motion == Motion::Static ? JPH::EActivation::DontActivate : JPH::EActivation::Activate);
        if (id.IsInvalid())
            return {};
        const BodyHandle handle = impl_->bodies.Insert({id, desc.shape, desc.motion});
        bi.SetUserData(id, Pack(handle));
        impl_->bodiesAdded = true;
        return handle;
    }

    bool PhysicsWorld::DestroyBody(BodyHandle body)
    {
        const Impl::Body *b = impl_->bodies.Get(body);
        if (b == nullptr)
            return false;
        JPH::BodyInterface &bi = impl_->system.GetBodyInterface();
        bi.RemoveBody(b->id);
        bi.DestroyBody(b->id);
        return impl_->bodies.Remove(body);
    }

    bool PhysicsWorld::GetBodyTransform(BodyHandle body, glm::vec3 &position, glm::quat &rotation) const
    {
        const Impl::Body *b = impl_->bodies.Get(body);
        if (b == nullptr)
            return false;
        JPH::RVec3 p;
        JPH::Quat q;
        impl_->system.GetBodyInterface().GetPositionAndRotation(b->id, p, q);
        position = FromJolt(JPH::Vec3(p));
        rotation = FromJolt(q);
        return true;
    }

    bool PhysicsWorld::SetBodyTransform(BodyHandle body, const glm::vec3 &position, const glm::quat &rotation, float deltaSeconds)
    {
        const Impl::Body *b = impl_->bodies.Get(body);
        if (b == nullptr)
            return false;
        JPH::BodyInterface &bi = impl_->system.GetBodyInterface();
        if (b->motion == Motion::Kinematic && deltaSeconds > 0.0F)
            bi.MoveKinematic(b->id, JPH::RVec3(ToJolt(position)), ToJolt(rotation), deltaSeconds);
        else
            bi.SetPositionAndRotation(b->id, JPH::RVec3(ToJolt(position)), ToJolt(rotation), JPH::EActivation::Activate);
        return true;
    }

    const Shape *PhysicsWorld::GetBodyShape(BodyHandle body) const
    {
        const Impl::Body *b = impl_->bodies.Get(body);
        return b ? &b->shape : nullptr;
    }

    std::size_t PhysicsWorld::BodyCount() const noexcept { return impl_->bodies.Size(); }

    std::optional<RayHit> PhysicsWorld::Raycast(const glm::vec3 &origin, const glm::vec3 &direction, float maxDistance) const
    {
        if (!Positive(maxDistance))
            return std::nullopt;
        const JPH::RRayCast ray{JPH::RVec3(ToJolt(origin)), ToJolt(direction * maxDistance)};
        JPH::RayCastResult result;
        if (!impl_->system.GetNarrowPhaseQuery().CastRay(ray, result))
            return std::nullopt;

        RayHit hit;
        hit.distance = result.mFraction * maxDistance;
        const JPH::RVec3 point = ray.GetPointOnRay(result.mFraction);
        hit.point = FromJolt(JPH::Vec3(point));
        const JPH::BodyLockRead lock(impl_->system.GetBodyLockInterface(), result.mBodyID);
        if (lock.Succeeded())
        {
            hit.normal = FromJolt(lock.GetBody().GetWorldSpaceSurfaceNormal(result.mSubShapeID2, point));
            hit.body = Unpack(lock.GetBody().GetUserData());
        }
        return hit;
    }

    CharacterHandle PhysicsWorld::CreateCharacter(const CharacterDesc &desc)
    {
        if (!Positive(desc.radius) || !Positive(desc.halfHeight))
            return {};
        JPH::CharacterVirtualSettings settings;
        settings.mShape = new JPH::CapsuleShape(desc.halfHeight, desc.radius);
        settings.mMaxSlopeAngle = JPH::DegreesToRadians(desc.maxSlopeDegrees);
        // Contacts below the lower hemisphere's center count as "standing on something".
        settings.mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisY(), -desc.halfHeight);
        JPH::Ref<JPH::CharacterVirtual> character =
            new JPH::CharacterVirtual(&settings, JPH::RVec3(ToJolt(desc.position)), JPH::Quat::sIdentity(), 0, &impl_->system);
        return impl_->characters.Insert({character, desc});
    }

    bool PhysicsWorld::DestroyCharacter(CharacterHandle character) { return impl_->characters.Remove(character); }

    bool PhysicsWorld::MoveCharacter(CharacterHandle character, const glm::vec3 &horizontalVelocity, bool jump, float deltaSeconds)
    {
        Impl::Character *c = impl_->characters.Get(character);
        if (c == nullptr || !(deltaSeconds > 0.0F))
            return false;
        JPH::CharacterVirtual &cv = *c->jolt;

        // 1. Vertical speed: zero while standing, jump speed on a jump, gravity while airborne.
        cv.UpdateGroundVelocity();
        const bool grounded = cv.GetGroundState() == JPH::CharacterBase::EGroundState::OnGround;
        float vertical = cv.GetLinearVelocity().GetY();
        if (grounded)
            vertical = jump ? jumpSpeed : 0.0F;
        if (!grounded || jump)
            vertical += gravity.y * deltaSeconds;
        cv.SetLinearVelocity(JPH::Vec3(horizontalVelocity.x, vertical, horizontalVelocity.z));

        // 2. Move: slide along walls, step up small ledges, stick to the floor going down slopes.
        JPH::CharacterVirtual::ExtendedUpdateSettings update;
        update.mWalkStairsStepUp = JPH::Vec3(0.0F, c->desc.stepHeight, 0.0F);
        cv.ExtendedUpdate(deltaSeconds, ToJolt(gravity), update, impl_->system.GetDefaultBroadPhaseLayerFilter(movingLayer),
                          impl_->system.GetDefaultLayerFilter(movingLayer), {}, {}, impl_->temp);
        return true;
    }

    bool PhysicsWorld::TeleportCharacter(CharacterHandle character, const glm::vec3 &position)
    {
        Impl::Character *c = impl_->characters.Get(character);
        if (c == nullptr)
            return false;
        c->jolt->SetPosition(JPH::RVec3(ToJolt(position)));
        c->jolt->SetLinearVelocity(JPH::Vec3::sZero());
        return true;
    }

    std::optional<CharacterState> PhysicsWorld::GetCharacter(CharacterHandle character) const
    {
        const Impl::Character *c = impl_->characters.Get(character);
        if (c == nullptr)
            return std::nullopt;
        return CharacterState{FromJolt(JPH::Vec3(c->jolt->GetPosition())), FromJolt(c->jolt->GetLinearVelocity()),
                              c->jolt->GetGroundState() == JPH::CharacterBase::EGroundState::OnGround};
    }

    std::size_t PhysicsWorld::CharacterCount() const noexcept { return impl_->characters.Size(); }

    void PhysicsWorld::Step(float deltaSeconds)
    {
        if (!(deltaSeconds > 0.0F))
            return;
        // Rebuilding the broad-phase tree after adding bodies keeps queries fast.
        if (impl_->bodiesAdded)
        {
            impl_->system.OptimizeBroadPhase();
            impl_->bodiesAdded = false;
        }
        impl_->system.SetGravity(ToJolt(gravity));
        impl_->system.Update(deltaSeconds, 1, &impl_->temp, &impl_->jobs);
    }

    void PhysicsWorld::ForEachBody(const std::function<void(BodyHandle, const Shape &, Motion, const glm::vec3 &, const glm::quat &)> &visit) const
    {
        impl_->bodies.ForEach([&](BodyHandle handle, const Impl::Body &body) {
            glm::vec3 position;
            glm::quat rotation;
            (void)GetBodyTransform(handle, position, rotation);
            visit(handle, body.shape, body.motion, position, rotation);
        });
    }
}
