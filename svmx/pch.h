#pragma once
#include <ntifs.h>
#include <minwindef.h>
#include <ntstrsafe.h>
#include <limits.h>
#include <intrin.h>
#include "int-ll64.h"
#include "types.h"
#include "Logging.h"
#include "msr-index.h"

#define DRIVER_TAG	'xmvs'

#include "kvm_host.h"
#include "virtext.h"
#include "msr.h"
#include "processor-flags.h"




#pragma warning(disable:4200)
#pragma warning(disable:4201)