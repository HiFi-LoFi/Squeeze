Squeeze
=======

This little JUCE add-on provides functionality for convenient parallelization
of common code constructs using a provided thread pool.

Why the name "squeeze": Because it can help to squeeze more Ju(i)ce out of
the CPU... (I know, I know, but it needed some name...)

## Features: ##

- Work balancing is performed for even usage of available CPU cores.

- It is tried to reduce over-subscription as much as possible (i.e. there are more
  threads than CPU cores).

- It is tried to live in harmony as much as possible with arbitrary other jobs
  using the same JUCE ThreadPool object (again to reduce over-subscription).

- Combining and nesting of parallelized code is allowed (e.g. juce::ThreadPoolJob
  containing ParallelInvoke() which contains ParallelFor() loops).

- Uses the quite liberal MIT license.

## How to use it in your project: ##

Currently it's just a header file, so just copy and include it to your project
(maybe I'll make it a JUCE module one day).

## Best practices: ##

- Parallelizing always comes with some additional overhead, so the parallelized
  code should contain sufficient workload (e.g. enough loop iterations and/or
  enough work per loop iteration).

- It is best used for computationally intensive code. It is probably not a good idea
  to use it for long running and blocking operations like IO.

- It is absolutely necessary that the parallelized code is suitable for parallelization.
  This means that either all work items are independent from each other, or that
  sufficient synchronization is used (e.g. critical sections etc., but which again
  comes with some performance costs).

- It is recommended to re-use the same thread pool as much as possible in order to
  avoid the expensive costs of thread creation (in fact, this and avoidance of
  over-subscription are the main reasons for a thread pool). For example, your
  application might create a thread pool on startup and re-use it during the whole
  application run.

## Attention: ##

- None of the parallelized code must throw exceptions!

- This library uses "modern" C++ features such as variadic templates etc. and requires
  at least a C++11-like STL implementation (currently, it's a funny mixture of JUCE and STL
  functionality: It gave me a good opportunity to get more familiar with some of the
  "new" C++ features).

- As JUCE is used quite a lot for real-time audio programming:
  Don't use this stuff in your audio callback, because it contains all sorts
  of potentially blocking constructs (mutexes, allocation, waits, ...)!


## ParallelFor() ##

- Parallelizes for-loops

- Works with random access iterators as well as signed/unsigned integer indices

- Currently, only "forward" iterating with step size 1 is supported (i.e. operator ++)

- An optional chunk size can be specified for fine-tuning the performance:
  - Smaller chunk size: Potentially better load balancing, but slightly larger overhead
  - Larger chunk size: Potentially worse load balancing, but slightly less overhead


## ParallelInvoke() ##

- Parallel execution of independent code blocks


## Example ##

    void Example()
    {
      std::vector<float> data(...);

      // Grab some thread pool
      juce::ThreadPool& threadPool = GetThreadPool();

      // Loop using iterators
      squeeze::ParallelFor(threadPool, begin(data), end(data), [&](std::vector<float>::iterator it)
      {
        (*it) = SomeCalculation(*it);
      });

      // Loop using indices
      squeeze::ParallelFor(threadPool, size_t(0), data.size(), [&](size_t i)
      {
        data[i] = AnotherCalculation(i);
      });

      // Performing three different calculations in parallel
      squeeze::ParallelInvoke(threadPool,
        [&]() { SomeCalculation(data); },
        [&]() { AnotherCalculation(data); },
        [&]() { YetAnotherCalculation(data); });
    }
