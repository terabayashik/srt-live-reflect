// stdafx.h : 標準のシステム インクルード ファイルのインクルード ファイル、または
// 参照回数が多く、かつあまり変更されない、プロジェクト専用のインクルード ファイル
// を記述します。
//

#pragma once

#ifdef WIN32
#define _CRT_SECURE_NO_WARNINGS 1
#endif

#include <stdio.h>
#include <stdint.h>

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <chrono>
#include <csignal>

#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>
#include <boost/tokenizer.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/thread.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/range/algorithm_ext/erase.hpp>
#include <boost/json.hpp>
#include <boost/date_time/c_local_time_adjustor.hpp>
#include <boost/date_time/local_time/local_time.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/xpressive/xpressive.hpp>

#include <curl/curl.h>
#include <srt.h>
