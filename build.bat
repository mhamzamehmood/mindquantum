@rem Copyright 2020 Huawei Technologies Co., Ltd
@rem
@rem Licensed under the Apache License, Version 2.0 (the "License");
@rem you may not use this file except in compliance with the License.
@rem You may obtain a copy of the License at
@rem
@rem http://www.apache.org/licenses/LICENSE-2.0
@rem
@rem Unless required by applicable law or agreed to in writing, software
@rem distributed under the License is distributed on an "AS IS" BASIS,
@rem WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
@rem See the License for the specific language governing permissions and
@rem limitations under the License.
@rem ============================================================================
@echo off
@title mindquantum_build

SET BASE_PATH=%CD%
SET BUILD_PATH=%BASE_PATH%/build
SET OUTPUT=%BASE_PATH%/output

IF NOT EXIST "%BUILD_PATH%" (
    md "build"
)

cd %BASE_PATH%

python -m pip install --user build

python -m build -w -C--global-option=--set -C--global-option=ENABLE_PROJECTQ -C--global-option=--unset -C--global-option=ENABLE_QUEST %*

IF NOT EXIST "%OUTPUT%" (
    md "output"
)

move -y %BASE_PATH%/dist/* %OUTPUT%
