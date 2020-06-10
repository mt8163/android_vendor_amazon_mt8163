#
# Copyright (C) 2019-2020 The Lineageos Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

LOCAL_PATH_VENDOR := vendor/amazon/mt8163

# Thermal Manager
PRODUCT_PACKAGES += \
   thermal_manager

# Email
PRODUCT_PACKAGES += \
   Email

# Debugging tool
PRODUCT_PACKAGES +=\
   debug_tool
   
# vold fix 
PRODUCT_PACKAGES += \
   vold_fix
   
# MediatekParts
PRODUCT_PACKAGES += \
   MediatekParts

# Kernel modules loader
PRODUCT_COPY_FILES += \
   $(LOCAL_PATH_VENDOR)/proprietary/external/insmod/init.insmod.sh:$(TARGET_COPY_OUT_VENDOR)/bin/init.insmod.sh \
   $(LOCAL_PATH_VENDOR)/proprietary/external/insmod/init.insmod.cfg:$(TARGET_COPY_OUT_VENDOR)/etc/init.insmod.cfg 

PRODUCT_PROPERTY_OVERRIDES  += \
   ro.config.hw_quickpoweron=true \
   ro.build.shutdown_timeout=0

