#pragma once
#pragma once

// Define M_PI if not already defined (Windows MSVC)
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Fix for OpenMP collapse directive
#ifdef _MSC_VER
#define OMP_PARALLEL_FOR _Pragma("omp parallel for")
#else
#define OMP_PARALLEL_FOR _Pragma("omp parallel for collapse(2)")
#endif