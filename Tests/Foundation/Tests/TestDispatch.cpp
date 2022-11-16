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

#ifndef DISABLE_UNIT_TESTS

#include <Aurora/Foundation/Dispatch.h>
#include <Aurora/Foundation/ResourcePool.h>
#include <algorithm>
#include <gtest/gtest.h>
#include <limits>
#include <random>

using namespace std;
using namespace Aurora::Foundation;

namespace
{

class DispatchTest : public ::testing::Test
{
public:
    DispatchTest() {}
    ~DispatchTest() {}

    // Optionally override the base class's setUp function to create some
    // data for the tests to use.
    virtual void SetUp() override {}

    // Optionally override the base class's tearDown to clear out any changes
    // and release resources.
    virtual void TearDown() override {}

    void testResourcePool(bool ordered);
};

} // namespace

shared_ptr<vector<int>> createRandomVector(int count)
{
    default_random_engine generator;
    uniform_int_distribution<int> distribution(0, 9);
    shared_ptr<vector<int>> intVec = make_shared<vector<int>>();
    int i                          = count;

    while (i--)
        intVec->push_back(distribution(generator));

    return intVec;
}

void validateVector(const shared_ptr<vector<int>>& intVec, vector<bool>& results)
{
    int minVal = numeric_limits<int>::min();

    // Verify that numerical values are stored in ascending order.
    for (auto& element : *intVec)
    {
        if (element == 0)
        {
            // Reset.
            minVal = numeric_limits<int>::min();
        }
        else if (element < minVal)
        {
            // Return failure.
            results.push_back(false);
            return;
        }
        else
        {
            minVal = element;
        }
    }

    // Return success.
    results.push_back(true);
}

void processVector(vector<int>& intVec, vector<int>& results)
{
    // Sort the vector.
    sort(intVec.begin(), intVec.end());

    // Add reset value.
    results.push_back(0);

    // Append to output.
    results.insert(results.end(), intVec.begin(), intVec.end());
}

TEST_F(DispatchTest, TestDispatch)
{
    shared_ptr<Aurora::Foundation::IDispatch> concurrentQ =
        Aurora::Foundation::IDispatch::createConcurrent();

    // Create vectors of random numbers.
    auto vecA = createRandomVector(40);
    auto vecB = createRandomVector(40);
    auto vecC = createRandomVector(40);

    // Create sorting tasks.
    auto sortA = [vecA]() { sort(vecA->begin(), vecA->end()); };
    auto sortB = [vecB]() { sort(vecB->begin(), vecB->end()); };
    auto sortC = [vecC]() { sort(vecC->begin(), vecC->end()); };

    // Execute sorting tasks asynchronously and concurrently.
    concurrentQ->dispatch(sortA);
    concurrentQ->dispatch(sortB);
    concurrentQ->dispatch(sortC);

    shared_ptr<Aurora::Foundation::IDispatch> serialQ =
        Aurora::Foundation::IDispatch::createSerial();

    // Nest the dispatch queues such that serial tasks will wait for all concurrent processing.
    auto waitTask = [concurrentQ]() { concurrentQ->wait(); };

    vector<bool> results;

    // Create serial validation tasks that store results.
    auto taskD = [vecA, &results]() { validateVector(vecA, results); };
    auto taskE = [vecB, &results]() { validateVector(vecB, results); };
    auto taskF = [vecC, &results]() { validateVector(vecC, results); };

    // Execute validation tasks asynchronously and serially.
    serialQ->dispatch(waitTask);
    serialQ->dispatch(taskD);
    serialQ->dispatch(taskE);
    serialQ->dispatch(taskF);

    // Wait for all tasks to complete (both concurrent and serial tasks).
    serialQ->wait();

    // Check final results.
    for (const auto& result : results)
        ASSERT_EQ(result, true);
}

void DispatchTest::testResourcePool(bool ordered)
{
    // Int vector resource.
    using Resource = vector<int>;

    // A shared set of resources.
    vector<shared_ptr<Resource>> resourceList;

    shared_ptr<Resource> outputA = make_shared<Resource>();
    shared_ptr<Resource> outputB = make_shared<Resource>();

    // Create a list of limited resources (int vectors).
    resourceList.push_back(outputA);
    resourceList.push_back(outputB);

    // The resource pool to manage the above.
    shared_ptr<ResourcePool<shared_ptr<Resource>>> resourcePool;

    if (ordered)
        resourcePool = make_shared<OrderedResourcePool<shared_ptr<Resource>>>(resourceList);
    else
        resourcePool = make_shared<UnorderedResourcePool<shared_ptr<Resource>>>(resourceList);

    // Create a concurrent dispatch queue.
    shared_ptr<Aurora::Foundation::IDispatch> concurrentQ =
        Aurora::Foundation::IDispatch::createConcurrent();

    const int kJobCount = 500;
    const int kJobSize  = 1000;

    // Create jobs for concurrent processing.
    for (int i = 0; i < kJobCount; i++)
    {
        // Create a vector of random numbers.
        auto vec = createRandomVector(kJobSize);

        // Get an order value to use for ordered pools.
        int order = i % static_cast<int>(resourceList.size());

        // Dispatch processing task using available string resources.
        concurrentQ->dispatch([vec, order, &resourcePool]() {
            // Get an output vector.
            shared_ptr<vector<int>> output = resourcePool->acquire(order);

            // Process the int vector.
            processVector(*vec, *output);

            // Release the output vector.
            resourcePool->release(output, order);
        });
    }

    // Wait for all encoding tasks to complete.
    concurrentQ->wait();

    // Validate output size.
    size_t total = 0;
    for (auto& vec : resourceList)
        total += vec->size();
    ASSERT_EQ(total, static_cast<size_t>(kJobCount * kJobSize + kJobCount));

    // Validate the output vectors.
    vector<bool> results;
    for (auto& vec : resourceList)
        validateVector(vec, results);

    // Finally, check the results.
    for (const auto& result : results)
        ASSERT_EQ(result, true);
}

TEST_F(DispatchTest, TestOrderedResourcePool)
{
    testResourcePool(true);
}

TEST_F(DispatchTest, TestUnorderedResourcePool)
{
    testResourcePool(false);
}

#endif
