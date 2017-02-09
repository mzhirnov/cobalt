#ifndef COBALT_PLATFORM_HPP_INCLUDED
#define COBALT_PLATFORM_HPP_INCLUDED

#pragma once

#include <boost/predef.h>

#define $on(...) __VA_ARGS__
#define $off(...)

////////////////////////////////////////////////////////////////////////////////
// Windows

#if BOOST_OS_WINDOWS
#include <winapifamily.h>
#define $windows $on
#if defined(WINAPI_FAMILY) && WINAPI_FAMILY == WINAPI_PARTITION_APP
#define $windows_app $on
#endif
#endif // BOOST_OS_WINDOWS

#ifdef $windows
#define $not_windows $off
#else
#define $windows $off
#define $not_windows $on
#endif // $windows

#ifdef $windows_app
#define $not_windows_app $off
#else
#define $windows_app $off
#define $not_windows_app $on
#endif // $windows_app

////////////////////////////////////////////////////////////////////////////////
// iOS

#if BOOST_OS_IOS
#include <TargetConditionals.h>
#define $ios $on
#if defined(TARGET_IPHONE_SIMULATOR) && TARGET_IPHONE_SIMULATOR
#define $ios_simulator $on
#endif
#endif // BOOST_OS_IOS

#ifdef $ios
#define $not_ios $off
#else
#define $ios $off
#define $not_ios $on
#endif // $ios

#ifdef $ios_simulator
#define $not_ios_simulator $off
#else
#define $ios_simulator $off
#define $not_ios_simulator $on
#endif // $ios_simulator

////////////////////////////////////////////////////////////////////////////////
// macOS

#if BOOST_OS_MACOS
#define $macos $on
#endif // BOOST_OS_MACOS

#ifdef $macos
#define $not_macos $off
#else
#define $macos $off
#define $not_macos $on
#endif // $macos

////////////////////////////////////////////////////////////////////////////////
// Android

#if BOOST_OS_ANDROID
#define $android $on
#endif // BOOST_OS_ANDROID

#ifdef $android
#define $not_android $off
#else
#define $android $off
#define $not_android $on
#endif // $android

////////////////////////////////////////////////////////////////////////////////
// Linux

#if BOOST_OS_LINUX
#define $linux $on
#endif // BOOST_OS_LINUX

#ifdef $linux
#define $not_linux $off
#else
#define $linux $off
#define $not_linux $on
#endif // $linux

////////////////////////////////////////////////////////////////////////////////
// Desktop | Mobile

#if BOOST_OS_WINDOWS || BOOST_OS_MACOS || BOOST_OS_LINUX
#define $desktop $on
#endif //

#ifdef $desktop
#define $mobile $off
#else
#define $desktop $off
#define $mobile $on
#endif // $linux

#endif // COBALT_PLATFORM_HPP_INCLUDED
