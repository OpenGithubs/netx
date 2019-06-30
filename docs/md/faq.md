# Frequently Asked Questions

<!--
@cond TURN_OFF_DOXYGEN
-->
# Table of Contents

* [Introduction](#introduction)
* [FAQ](#faq)
  * [Why is my debug build on Windows so slow?](#why-is-my-debug-build-on-windows-so-slow)
  * [How can I represent hierarchies with my components?](#how-can-i-represent-hierarchies-with-my-components)
<!--
@endcond TURN_OFF_DOXYGEN
-->

# Introduction

This is a constantly updated section where I'll try to put the answers to the
most frequently asked questions.<br/>
If you don't find your answer here, there are two cases: nobody has done it yet
or this section needs updating. In both cases, try to
[open a new issue](https://github.com/skypjack/entt/issues/new) or enter the
[gitter channel](https://gitter.im/skypjack/entt) and ask your question.
Probably someone already has an answer for you and we can then integrate this
part of the documentation.

# FAQ

## Why is my debug build on Windows so slow?

`EnTT` is an experimental project that I also use to keep me up-to-date with the
latest revision of the language and the standard library. For this reason, it's
likely that some classes you're working with are using standard containers under
the hood.<br/>
Unfortunately, it's known that the standard containers aren't particularly
performing in debugging (the reasons for this go beyond this document) and are
even less so on Windows apparently. Fortunately this can also be mitigated a
lot, achieving good results in many cases.

First of all, there are two things to do in a Windows project:

* Disable the [`/JMC`](https://docs.microsoft.com/cpp/build/reference/jmc)
  option (_Just My Code_ debugging), available starting in Visual Studio 2017
  version 15.8.

* Set the [`_ITERATOR_DEBUG_LEVEL`](https://docs.microsoft.com/cpp/standard-library/iterator-debug-level)
  macro to 0. This will disable checked iterators and iterator debugging.

Moreover, the macro `ENTT_DISABLE_ASSERT` should be defined to disable internal
checks made by `EnTT` in debug. These are asserts introduced to help the users,
but require to access to the underlying containers and therefore risk ruining
the performance in some cases.

With these changes, debug performance should increase enough for most cases. If
you want something more, you can can also switch to an optimization level `O0`
or preferably `O1`.

## How can I represent hierarchies with my components?

This is one of the first questions that anyone makes when starting to work with
the entity-component-system architectural pattern.<br/>
There are several approaches to the problem and what’s the best one depends
mainly on the real problem one is facing. In all cases, how to do it doesn't
strictly depend on the library in use, but the latter can certainly allow or
not different techniques depending on how the data are laid out.

I tried to describe some of the techniques that fit well with the model of
`EnTT`. [Here](https://skypjack.github.io/2019-06-25-ecs-baf-part-4/) is the
first post of a series that tries to explore the problem. More will probably
come in future.

Long story short, you can always define a tree where the nodes expose implicit
lists of children by means of the following type:

```cpp
struct relationship {
    std::size_t children{};
    entt::entity first{entt::null};
    entt::entity prev{entt::null};
    entt::entity next{entt::null};
    entt::entity parent{entt::null};
    // ... other data members ...
};
```

The sort functionalities of `EnTT`, the groups and all the other features of the
library can help then to get the best in terms of data locality and therefore
performance from this component.
