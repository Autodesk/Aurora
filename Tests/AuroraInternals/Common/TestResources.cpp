// Copyright 2022 Autodesk, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#if !defined(DISABLE_UNIT_TESTS)

#include "TestHelpers.h"
#include <gtest/gtest.h>

#include <fstream>
#include <regex>
#include <streambuf>

// Include the Aurora PCH (this is an internal test so needs all the internal Aurora includes)
#include "pch.h"

#include "ResourceStub.h"

namespace
{

// Test fixture accessing test asset data path.
class ResourcesTest : public ::testing::Test
{
public:
    ResourcesTest() : _dataPath(TestHelpers::kSourceRoot + "/Renderers/Tests/Data") {}
    ~ResourcesTest() {}
    const std::string& dataPath() { return _dataPath; }

protected:
    std::string _dataPath;
};

struct Bar;
struct Foo
{
    shared_ptr<Bar> bar;
};
struct Bar
{
    int bloop;
};

class BarResource : public Aurora::ResourceStub
{
public:
    BarResource(const Aurora::Path& path, const Aurora::ResourceMap& container) :
        Aurora::ResourceStub(path, container)
    {
        numBars++;
        initializeIntApplicators(
            { { "bloop", [this](string, int val) { _resource->bloop = val; } } });
    }
    virtual ~BarResource()
    {
        shutdown();
        numBars--;
    }

    static int numBars;

    void createResource() override { _resource = make_shared<Bar>(); }
    void destroyResource() override { _resource.reset(); }
    const ResourceType& type() override { return resourceType; }
    shared_ptr<Bar> resource() { return _resource; }

    static constexpr ResourceType resourceType = ResourceType::Environment;

private:
    shared_ptr<Bar> _resource;
};
int BarResource::numBars = 0;

class FooResource : public Aurora::ResourceStub
{
public:
    FooResource(const Aurora::Path& path, const Aurora::ResourceMap& container) :
        Aurora::ResourceStub(path, container)
    {
        numFoos++;
        initializePathApplicators({ { "bar", [this](string propName, Aurora::Path) {
                                         _resource->bar =
                                             getReferenceResource<BarResource, Bar>(propName);
                                     } } });
    }
    virtual ~FooResource()
    {
        shutdown();
        numFoos--;
    }

    void createResource() override { _resource = make_shared<Foo>(); }
    void destroyResource() override { _resource.reset(); }
    const ResourceType& type() override { return resourceType; }

    static int numFoos;
    static constexpr ResourceType resourceType = ResourceType::GroundPlane;

private:
    shared_ptr<Foo> _resource;
};

int FooResource::numFoos = 0;

// Basic resources test.
TEST_F(ResourcesTest, BasicTest)
{
    Aurora::ResourceMap resources;
    resources["bar0"] = make_shared<BarResource>("bar0", resources);
    resources["bar1"] = make_shared<BarResource>("bar1", resources);
    ASSERT_EQ(BarResource::numBars, 2);

    ASSERT_FALSE(resources["bar0"]->isActive());
    ASSERT_FALSE(resources["bar1"]->isActive());
    resources["bar0"]->setProperties({ { "bloop", 42 } });
    resources["bar1"]->setProperties({ { "bloop", 1 } });

    Aurora::ResourceStubPtr pFoo0 = make_shared<FooResource>("foo0", resources);
    ASSERT_FALSE(pFoo0->isActive());
    pFoo0->incrementPermanentRefCount();
    ASSERT_TRUE(pFoo0->isActive());
    ASSERT_EQ(FooResource::numFoos, 1);

    pFoo0->setProperties({ { "bar", "bar0" } });
    ASSERT_TRUE(resources["bar0"]->isActive());
    ASSERT_FALSE(resources["bar1"]->isActive());

    shared_ptr<Bar> pFoo0Bar;
    pFoo0Bar = pFoo0->getReferenceResource<BarResource, Bar>("bar");
    ASSERT_EQ(pFoo0Bar->bloop, 42);

    pFoo0->setProperties({ { "bar", "bar1" } });
    ASSERT_FALSE(resources["bar0"]->isActive());
    ASSERT_TRUE(resources["bar1"]->isActive());

    pFoo0Bar = pFoo0->getReferenceResource<BarResource, Bar>("bar");
    ASSERT_EQ(pFoo0Bar->bloop, 1);

    Aurora::ResourceStubPtr pFoo1 = make_shared<FooResource>("foo1", resources);
    ASSERT_FALSE(pFoo1->isActive());
    ASSERT_EQ(FooResource::numFoos, 2);

    pFoo1->setProperties({ { "bar", "bar1" } });
    ASSERT_FALSE(resources["bar0"]->isActive());
    ASSERT_TRUE(resources["bar1"]->isActive());

    pFoo1->setProperties({ { "bar", "bar0" } });
    ASSERT_FALSE(resources["bar0"]->isActive());
    ASSERT_TRUE(resources["bar1"]->isActive());

    pFoo1->incrementPermanentRefCount();
    ASSERT_TRUE(pFoo1->isActive());
    ASSERT_TRUE(resources["bar0"]->isActive());
    ASSERT_TRUE(resources["bar1"]->isActive());

    pFoo0->decrementPermanentRefCount();
    ASSERT_FALSE(pFoo0->isActive());
    ASSERT_TRUE(resources["bar0"]->isActive());
    ASSERT_FALSE(resources["bar1"]->isActive());
}

struct Bloop
{
    vector<shared_ptr<Bar>> bars;
};

class BloopResource : public Aurora::ResourceStub
{
public:
    BloopResource(const Aurora::Path& path, const Aurora::ResourceMap& container) :
        Aurora::ResourceStub(path, container)
    {
        numBloops++;
        initializePathArrayApplicators({ { "bars", [this](string propName, vector<Aurora::Path>) {
                                              _resource->bars =
                                                  getReferenceResources<BarResource, Bar>(propName);
                                          } } });
    }
    virtual ~BloopResource()
    {
        shutdown();
        numBloops--;
    }

    void createResource() override { _resource = make_shared<Bloop>(); }
    void destroyResource() override { _resource.reset(); }
    const ResourceType& type() override { return resourceType; }
    static constexpr ResourceType resourceType = ResourceType::Image;

    static int numBloops;
    shared_ptr<Bloop> resource() { return _resource; }

private:
    shared_ptr<Bloop> _resource;
};

int BloopResource::numBloops = 0;

TEST_F(ResourcesTest, ArrayResourceTest)
{
    Aurora::ResourceMap resources;
    resources["bar0"] = make_shared<BarResource>("bar0", resources);
    resources["bar1"] = make_shared<BarResource>("bar1", resources);
    ASSERT_EQ(BarResource::numBars, 2);
    ASSERT_FALSE(resources["bar0"]->isActive());
    ASSERT_FALSE(resources["bar1"]->isActive());

    shared_ptr<BloopResource> pBloop0 = make_shared<BloopResource>("bloop0", resources);
    pBloop0->incrementPermanentRefCount();
    pBloop0->setProperties({ { "bars", vector<Aurora::Path>({ "bar0", "bar1" }) } });
    ASSERT_EQ(pBloop0->resource()->bars.size(), 2);
    ASSERT_TRUE(resources["bar0"]->isActive());
    ASSERT_TRUE(resources["bar1"]->isActive());

    pBloop0->setProperties({ { "bars", vector<Aurora::Path>({ "bar1" }) } });
    ASSERT_EQ(pBloop0->resource()->bars.size(), 1);
    ASSERT_FALSE(resources["bar0"]->isActive());
    ASSERT_TRUE(resources["bar1"]->isActive());

    pBloop0->setProperties({ { "bars", vector<Aurora::Path>() } });
    ASSERT_EQ(pBloop0->resource()->bars.size(), 0);
    ASSERT_FALSE(resources["bar0"]->isActive());
    ASSERT_FALSE(resources["bar1"]->isActive());
}

struct Flub
{
    vec2 uv;
};

class FlubResource : public Aurora::ResourceStub
{
public:
    FlubResource(const Aurora::Path& path, const Aurora::ResourceMap& container,
        const TypedResourceTracker<FlubResource, Flub>& tracker) :
        Aurora::ResourceStub(path, container, tracker.tracker())
    {
        initializeVec2Applicators(
            { { "uv", [this](string, glm::vec2 val) { _resource->uv = val; } } });
    }
    virtual ~FlubResource() { shutdown(); }

    void createResource() override { _resource = make_shared<Flub>(); }
    void destroyResource() override { _resource.reset(); }
    const ResourceType& type() override { return resourceType; }
    shared_ptr<Flub> resource() const { return _resource; }

    static constexpr ResourceType resourceType = ResourceType::Geometry;

private:
    shared_ptr<Flub> _resource;
};

// Tracker resources test.
TEST_F(ResourcesTest, TrackerTest)
{
    TypedResourceTracker<FlubResource, Flub> tracker;
    Aurora::ResourceMap resources;
    shared_ptr<FlubResource> pFlub0 = make_shared<FlubResource>("flub0", resources, tracker);
    shared_ptr<FlubResource> pFlub1 = make_shared<FlubResource>("flub1", resources, tracker);
    ASSERT_FALSE(tracker.changed());

    tracker.update();
    ResourceNotifier<Flub>& notifier = tracker.active();
    ASSERT_EQ(notifier.count(), 0);
    ASSERT_TRUE(notifier.modified());

    pFlub0->setProperties({ { "uv", vec2(0.5, 0.5) } });
    ASSERT_TRUE(tracker.changed());

    // Activate resource now stuff has changed.
    pFlub0->incrementPermanentRefCount();
    ASSERT_TRUE(tracker.changed());
    tracker.update();
    ASSERT_EQ(notifier.count(), 1);
    ASSERT_TRUE(notifier.modified());
    auto& activeRes = notifier.resources();
    ASSERT_EQ(activeRes[0].ptr(), pFlub0->resource().get());
    ASSERT_TRUE(tracker.changed());

    // Update again, active count remains at one, but no changes.
    tracker.update();
    ASSERT_EQ(notifier.count(), 1);
    ASSERT_TRUE(notifier.modified());
    ASSERT_TRUE(tracker.changed());

    // Modify active resource, changes reported.
    pFlub0->setProperties({ { "uv", vec2(0.75, 0.5) } });
    ASSERT_TRUE(tracker.changed());

    tracker.update();
    ASSERT_EQ(notifier.count(), 1);
    ASSERT_TRUE(notifier.modified());
    ASSERT_TRUE(tracker.changed());

    pFlub1->incrementPermanentRefCount();
    ASSERT_TRUE(tracker.changed());

    tracker.update();
    ASSERT_EQ(notifier.count(), 2);
    ASSERT_EQ(notifier.resources()[0].ptr(), pFlub0->resource().get());
    ASSERT_EQ(notifier.resources()[1].ptr(), pFlub1->resource().get());
    ASSERT_TRUE(notifier.modified());
    ASSERT_TRUE(tracker.changed());

    pFlub0->decrementPermanentRefCount();
    pFlub1->decrementPermanentRefCount();
    ASSERT_TRUE(tracker.changed());
    tracker.update();
    ASSERT_TRUE(tracker.changed());
    ASSERT_EQ(notifier.count(), 0);
}

size_t hashFlub(const Flub& flub)
{
    return hash<float> {}(flub.uv.x) ^ hash<float> {}(flub.uv.y);
}

TEST_F(ResourcesTest, HashLookupTest)
{
    TypedResourceTracker<FlubResource, Flub> tracker;
    Aurora::ResourceMap resources;
    shared_ptr<FlubResource> pFlub0 = make_shared<FlubResource>("flub0", resources, tracker);
    shared_ptr<FlubResource> pFlub1 = make_shared<FlubResource>("flub1", resources, tracker);
    shared_ptr<FlubResource> pFlub2 = make_shared<FlubResource>("flub2", resources, tracker);
    pFlub0->incrementPermanentRefCount();
    pFlub1->incrementPermanentRefCount();
    pFlub2->incrementPermanentRefCount();
    pFlub0->resource()->uv = vec2(0.1, 0.1);
    pFlub1->resource()->uv = vec2(0.1, 0.1);
    pFlub2->resource()->uv = vec2(0.5, 0.1);
    UniqueHashLookup<Flub, hashFlub> lookup;
    lookup.add(*pFlub0->resource());
    lookup.add(*pFlub1->resource());
    lookup.add(*pFlub2->resource());

    ASSERT_EQ(lookup.unique().size(), 2);
    ASSERT_EQ(lookup.unique()[0].ptr(), pFlub0->resource().get());
    ASSERT_EQ(lookup.unique()[1].ptr(), pFlub2->resource().get());
    ASSERT_EQ(lookup.count(), 3);
    ASSERT_EQ(lookup.getUniqueIndex(0), 0);
    ASSERT_EQ(lookup.getUniqueIndex(1), 0);
    ASSERT_EQ(lookup.getUniqueIndex(2), 1);

    lookup.clear();
    ASSERT_EQ(lookup.unique().size(), 0);
    ASSERT_EQ(lookup.count(), 0);
}

} // namespace

#endif
