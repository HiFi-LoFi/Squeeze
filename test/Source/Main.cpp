#include "../JuceLibraryCode/JuceHeader.h"

#include "../../Squeeze.h"

#include <array>
#include <random>
#include <string>
#include <vector>


//#define WAIT_FOR_KEY


inline float BurnCylces(float x)
{
  return std::cos(std::cos(std::cos(std::cos(std::cos(std::cos(x + 1.0f))))));
}


class TestJobWithLoop : public juce::ThreadPoolJob
{
public:
  TestJobWithLoop(juce::ThreadPool& threadPool, bool parallelLoop) :
    juce::ThreadPoolJob("TestJobWithParallelFor"),
    _threadPool(threadPool),
    _parallelLoop(parallelLoop)
  {
  }

  virtual JobStatus runJob() override
  {
    std::vector<float> data(100000);
    
    if (_parallelLoop)
    {
      squeeze::ParallelFor(_threadPool, size_t(0), data.size(), [&](size_t i)
      {
        data[i] = BurnCylces(static_cast<float>(i));
      });
    }
    else
    {
      for (size_t i = 0; i < data.size(); ++i)
      {
        data[i] = BurnCylces(static_cast<float>(i));
      }
    }
    
    return jobHasFinished;
  }

private:
  juce::ThreadPool& _threadPool;
  const bool _parallelLoop;
};


// ================================================================


static void TestParallelFor(juce::ThreadPool& threadPool)
{
  // Unsigned index
  {
    std::vector<unsigned> data(100000);
    squeeze::ParallelFor(threadPool, 0U, static_cast<unsigned>(data.size()), [&](unsigned i)
    {
      data[i] = i;
    });

    bool passed = true;
    for (unsigned i = 0; i < static_cast<unsigned>(data.size()); ++i)
    {
      if (data[i] != i)
      {
        passed = false;
      }
    }
    std::cout << "ParallelFor() with unsigned index: " << (passed ? "OK" : "FAILED") << std::endl;
  }

  // Signed index
  {
    std::vector<int> data(100000);
    squeeze::ParallelFor(threadPool, 0, static_cast<int>(data.size()), [&](int i)
    {
      data[i] = i;
    });

    bool passed = true;
    for (int i = 0; i < static_cast<int>(data.size()); ++i)
    {
      if (data[i] != i)
      {
        passed = false;
      }
    }
    std::cout << "ParallelFor() with signed index: " << (passed ? "OK" : "FAILED") << std::endl;
  }

  // Iterators
  {
    std::vector<size_t> data(100000);
    squeeze::ParallelFor(threadPool, begin(data), end(data), [&](const std::vector<size_t>::iterator& it)
    {
      (*it) = std::distance(begin(data), it);
    });

    bool passed = true;
    for (size_t i = 0; i < data.size(); ++i)
    {
      if (data[i] != i)
      {
        passed = false;
      }
    }
    std::cout << "ParallelFor() with iterator: " << (passed ? "OK" : "FAILED") << std::endl;
  }

  // Jobs with parallel loop
  {
    std::array<int, 2> durationMS;
    
    for (size_t parallel = 0; parallel < 2; ++parallel)
    {
#if defined(DEBUG)
      const size_t numberJobs = 100;
#else
      const size_t numberJobs = 1000;
#endif
      std::vector<std::unique_ptr<TestJobWithLoop>> jobs(numberJobs);
      
      auto timeStart = std::chrono::high_resolution_clock::now();
      for (size_t i = 0; i < jobs.size(); ++i)
      {
        jobs[i].reset(new TestJobWithLoop(threadPool, i == 1));
        threadPool.addJob(jobs[i].get(), false);
      }

      for (auto& job : jobs)
      {
        threadPool.waitForJobToFinish(job.get(), std::numeric_limits<int>::max());
      }
      
      durationMS[parallel] = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - timeStart).count());
    }
    
    std::cout << "Jobs with loop working (seq.: " << durationMS[0] << " ms / ParallelFor(): " << durationMS[1] << " ms)" << std::endl;
  }
  
  // Much work per iteration
  {
    std::cout << "======================================" << std::endl;
    std::cout << "ParallelFor(): Much work per iteration" << std::endl;
    std::cout << "======================================" << std::endl;

#if defined(DEBUG)
    const size_t minSize = 2;
    const size_t maxSize = 256;
#else
    const size_t minSize = 1;
    const size_t maxSize = 4096;
#endif

    for (size_t n = minSize; n <= maxSize; n *= 2)
    {
      const size_t iterations = maxSize / (n / minSize) * 3;

      int durationSequential = -1;
      int durationParallelFor = -1;
      int durationOpenMP = -1;
      bool passed = false;

      auto func = [](std::vector<float>& data)
      {
        for (size_t i = 0; i < data.size(); ++i)
        {
          data[i] = BurnCylces(static_cast<float>(i));
        }
        std::sort(begin(data), end(data));
      };

      std::vector<std::vector<float>> dataSequential(n, std::vector<float>(1000));
      {
        auto timeStart = std::chrono::high_resolution_clock::now();
        for (size_t iteration = 0; iteration < iterations; ++iteration)
        {
          for (auto& current : dataSequential)
          {
            func(current);
          }
        }
        durationSequential = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - timeStart).count());
      }

      {
        std::vector<std::vector<float>> data(dataSequential.size(), std::vector<float>(dataSequential[0].size()));
        auto timeStart = std::chrono::high_resolution_clock::now();
        for (size_t iteration = 0; iteration < iterations; ++iteration)
        {
          squeeze::ParallelFor(threadPool, size_t(0), data.size(), [&](size_t i)
          {
            func(data[i]);
          });
        }
        durationParallelFor = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - timeStart).count());
        passed = (data == dataSequential);
      }

#if defined(_OPENMP)
      {
        std::vector<std::vector<float>> data(dataSequential.size(), std::vector<float>(dataSequential[0].size()));
        auto timeStart = std::chrono::high_resolution_clock::now();
        for (size_t iteration = 0; iteration < iterations; ++iteration)
        {
          #pragma omp parallel for
          for (int i = 0; i < static_cast<int>(data.size()); ++i)
          {
            func(data[i]);
          }
        }
        durationOpenMP = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - timeStart).count());
      }
#endif // _OPENMP

      std::cout << n << " iterations: " << (passed ? "OK" : "FAILED") << " => Seq.: " << durationSequential << " ms / ParallelFor(): " << durationParallelFor << " ms / OpenMP: " << durationOpenMP << " ms" << std::endl;
    }
  }
  
  // Little work per iteration
  {
    std::cout << "========================================" << std::endl;
    std::cout << "ParallelFor(): Little work per iteration" << std::endl;
    std::cout << "========================================" << std::endl;

#if defined(DEBUG)
    const size_t minSize = 1000;
    const size_t maxSize = 1000000;
#else
    const size_t minSize = 1000;
    const size_t maxSize = 10000000;
#endif    
    
    for (size_t n = minSize; n <= maxSize; n *= 10)
    {
      const size_t iterations = (maxSize / (n / minSize)) / 250;
      std::vector<float> dataSequential(n);

      int durationSequential = -1;
      int durationParallelFor = -1;
      int durationOpenMP = -1;
      bool passed = false;

      {
        auto timeStart = std::chrono::high_resolution_clock::now();
        for (size_t iteration = 0; iteration < iterations; ++iteration)
        {
          for (size_t i = 0; i < dataSequential.size(); ++i)
          {
            dataSequential[i] = BurnCylces(static_cast<float>(i));
          }
        }
        durationSequential = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - timeStart).count());
      }
      {
        std::vector<float> data(dataSequential.size());
        auto timeStart = std::chrono::high_resolution_clock::now();
        for (size_t iteration = 0; iteration < iterations; ++iteration)
        {
          squeeze::ParallelFor(threadPool, size_t(0), data.size(), [&](size_t i)
          {
            data[i] = BurnCylces(static_cast<float>(i));
          });
        }
        durationParallelFor = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - timeStart).count());
        passed = (data == dataSequential);
      }
#if defined(_OPENMP)
      {
        std::vector<float> data(dataSequential.size());
        auto timeStart = std::chrono::high_resolution_clock::now();
        for (size_t iteration = 0; iteration < iterations; ++iteration)
        {
          #pragma omp parallel for
          for (int i = 0; i < static_cast<int>(data.size()); ++i)
          {
            data[i] = BurnCylces(static_cast<float>(i));
          }
        }
        durationOpenMP = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - timeStart).count());
      }
#endif // _OPENMP

      std::cout << n << " iterations: " << (passed ? "OK" : "FAILED") << " => Seq.: " << durationSequential << " ms / ParallelFor(): " << durationParallelFor << " ms / OpenMP: " << durationOpenMP << " ms" << std::endl;
    }
  }
}



void TestParallelInvoke(juce::ThreadPool& threadPool)
{
  // Single task
  {
    std::thread::id threadId;
    squeeze::ParallelInvoke(threadPool, [&]()
    {
      threadId = std::this_thread::get_id();
    });
    std::cout << "ParallelInvoke() with one task: " << (threadId == std::this_thread::get_id() ? "OK" : "FAILED") << std::endl;
  }

  // Several tasks
  {
    auto func = [](std::vector<float>& data, size_t n)
    {
      data.resize(n);
      for (size_t i = 0; i < data.size(); ++i)
      {
        data[i] = BurnCylces(static_cast<float>(i + 1));
      }
      std::reverse(begin(data), end(data));
      std::sort(begin(data), end(data));      
    };

    std::vector<std::vector<float>> dataSequential(4);
    std::vector<std::vector<float>> dataParallel = dataSequential;

    // Sequential
    auto timeStartSequential = std::chrono::high_resolution_clock::now();
    func(dataSequential[0], 1000000);
    func(dataSequential[1], 1000000);
    func(dataSequential[2], 1000000);
    func(dataSequential[3], 1000000);
    auto durationSequentialMS = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - timeStartSequential).count();

    // Parallel
    {
      auto timeStart = std::chrono::high_resolution_clock::now();
      squeeze::ParallelInvoke(threadPool,
        [&]() { func(dataParallel[0], 1000000); },
        [&]() { func(dataParallel[1], 1000000); },
        [&]() { func(dataParallel[2], 1000000); },
        [&]() { func(dataParallel[3], 1000000); });
      auto durationMS = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - timeStart).count();
      auto result = (dataParallel == dataSequential) ? "OK" : "FAILED";
      std::cout << "ParallelInvoke() (4 jobs of same size): " << result << " / " << durationMS << " ms (sequential: " << durationSequentialMS << " ms)" << std::endl;
    }
  }
  
  // Several tasks
  {
    auto func = [](std::vector<float>& data, size_t n)
    {
      data.resize(n);
      for (size_t i = 0; i < data.size(); ++i)
      {
        data[i] = BurnCylces(static_cast<float>(i + 1));
      }
      std::reverse(begin(data), end(data));
      std::sort(begin(data), end(data));
    };
    
    std::vector<std::vector<float>> dataSequential(24);
    std::vector<std::vector<float>> dataParallel = dataSequential;

    // Sequential
    auto timeStartSequential = std::chrono::high_resolution_clock::now();
    {
      func(dataSequential[0], 100000);
      func(dataSequential[1], 200000);
      func(dataSequential[2], 300000);
      func(dataSequential[3], 400000);
      func(dataSequential[4], 300000);
      func(dataSequential[5], 200000);
      func(dataSequential[6], 100000);
      func(dataSequential[7], 200000);
      func(dataSequential[8], 300000);
      func(dataSequential[9], 400000);
#if !defined(DEBUG)
      func(dataSequential[10], 500000);
      func(dataSequential[11], 800000);
      func(dataSequential[12], 900000);
      func(dataSequential[13], 300000);
      func(dataSequential[14], 200000);
      func(dataSequential[15], 100000);
      func(dataSequential[16], 500000);
      func(dataSequential[17], 100000);
      func(dataSequential[18], 500000);
      func(dataSequential[19], 300000);
      func(dataSequential[20], 500000);
      func(dataSequential[21], 100000);
      func(dataSequential[22], 900000);
      func(dataSequential[23], 100000);
#endif
    }
    auto durationSequentialMS = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - timeStartSequential).count();
    
    // Parallel
    {
      auto timeStart = std::chrono::high_resolution_clock::now();
      squeeze::ParallelInvoke(threadPool
                               ,[&]() { func(dataParallel[0], 100000); }
                               ,[&]() { func(dataParallel[1], 200000); }
                               ,[&]() { func(dataParallel[2], 300000); }
                               ,[&]() { func(dataParallel[3], 400000); }
                               ,[&]() { func(dataParallel[4], 300000); }
                               ,[&]() { func(dataParallel[5], 200000); }
                               ,[&]() { func(dataParallel[6], 100000); }
                               ,[&]() { func(dataParallel[7], 200000); }
                               ,[&]() { func(dataParallel[8], 300000); }
                               ,[&]() { func(dataParallel[9], 400000); }
#if !defined(DEBUG)
                               ,[&]() { func(dataParallel[10], 500000); }
                               ,[&]() { func(dataParallel[11], 800000); }
                               ,[&]() { func(dataParallel[12], 900000); }
                               ,[&]() { func(dataParallel[13], 300000); }
                               ,[&]() { func(dataParallel[14], 200000); }
                               ,[&]() { func(dataParallel[15], 100000); }
                               ,[&]() { func(dataParallel[16], 500000); }
                               ,[&]() { func(dataParallel[17], 100000); }
                               ,[&]() { func(dataParallel[18], 500000); }
                               ,[&]() { func(dataParallel[19], 300000); }
                               ,[&]() { func(dataParallel[20], 500000); }
                               ,[&]() { func(dataParallel[21], 100000); }
                               ,[&]() { func(dataParallel[22], 900000); }
                               ,[&]() { func(dataParallel[23], 100000); }
#endif
                               );
      auto durationParallelMS = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - timeStart).count();
      auto result = (dataParallel == dataSequential) ? "OK" : "FAILED";
      std::cout << "ParallelInvoke() (Many jobs of different size): " << result << " / " << durationParallelMS << " ms (sequential: " << durationSequentialMS << " ms)" << std::endl;      
    }
  }
  
  // Task with parallel loop
  {
    std::vector<std::vector<float>> dataSequential(12);
    std::vector<std::vector<float>> dataParallel = dataSequential;

    // Sequential
    auto timeStartSequential = std::chrono::high_resolution_clock::now();
    {
      auto func = [&](std::vector<float>& data, size_t n)
      {
        data.resize(n);
        for (size_t i = 0; i < data.size(); ++i)
        {
          data[i] = BurnCylces(static_cast<float>(i));
        }
        std::reverse(begin(data), end(data));
        std::sort(begin(data), end(data));
      };
      func(dataSequential[0], 100000);
      func(dataSequential[1], 100000);
      func(dataSequential[2], 100000);
      func(dataSequential[3], 100000);
      func(dataSequential[4], 100000);
      func(dataSequential[5], 100000);
      func(dataSequential[6], 100000);
      func(dataSequential[7], 100000);
      func(dataSequential[8], 100000);
      func(dataSequential[9], 100000);
      func(dataSequential[10], 100000);
      func(dataSequential[11], 100000);
    }
    auto durationSequentialMS = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - timeStartSequential).count();

    // Parallel
    {
      auto func = [&](std::vector<float>& data, size_t n)
      {
        data.resize(n);
        squeeze::ParallelFor(threadPool, size_t(0), data.size(), [&](size_t i)
        {
          data[i] = BurnCylces(static_cast<float>(i));
        });
        std::reverse(begin(data), end(data));
        std::sort(begin(data), end(data));
      };

      auto timeStart = std::chrono::high_resolution_clock::now();
      squeeze::ParallelInvoke(threadPool,
        [&]() { func(dataParallel[0], 100000); },
        [&]() { func(dataParallel[1], 100000); },
        [&]() { func(dataParallel[2], 100000); },
        [&]() { func(dataParallel[3], 100000); },
        [&]() { func(dataParallel[4], 100000); },
        [&]() { func(dataParallel[5], 100000); },
        [&]() { func(dataParallel[6], 100000); },
        [&]() { func(dataParallel[7], 100000); },
        [&]() { func(dataParallel[8], 100000); },
        [&]() { func(dataParallel[9], 100000); },
        [&]() { func(dataParallel[10], 100000); },
        [&]() { func(dataParallel[11], 100000); });
      auto durationMS = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - timeStart).count();
      auto result = (dataParallel == dataSequential) ? "OK" : "FAILED";
      std::cout << "ParallelInvoke() with ParallelFor(): " << result << " / " << durationMS << " ms (sequential: " << durationSequentialMS << " ms)" << std::endl;
    }
  }
}


//==============================================================================


int main(int, char**)
{
  juce::ThreadPool threadPool;

  // Heat the thread pools
  {
    std::vector<int> data(10000);
    squeeze::ParallelFor(threadPool, begin(data), end(data), [&](const std::vector<int>::iterator& it)
    {
      (*it) += 123;
    });

#if defined(_OPENMP)
    #pragma omp parallel for
    for (int i = 0; i < static_cast<int>(data.size()); ++i)
    {
      data[i] += 123;
    }
#endif // _OPENMP
  }
  
  TestParallelFor(threadPool);
  TestParallelInvoke(threadPool);

#if defined(WAIT_FOR_KEY)
  std::cout << "Press any key...";
  std::cin.get();
#endif
  
  return 0;
}
