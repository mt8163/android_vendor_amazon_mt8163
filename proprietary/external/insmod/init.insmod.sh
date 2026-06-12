#! /vendor/bin/sh

#########################################
### init.insmod.cfg format:           ###
### --------------------------------- ###
### [insmod|setprop] [path|prop name] ###
### ...                               ###
#########################################

cfg_file="/vendor/etc/init.insmod.cfg"
load_file="/vendor/lib/modules/modules.load"

kmsg_log() {
  echo "init.insmod: $1" > /dev/kmsg
}

kmsg_log "Starting module loading..."

if [ -f $cfg_file ]; then
  kmsg_log "Processing config file: $cfg_file"
  while IFS=" " read -r action name
  do
    case $action in
      "insmod")
        kmsg_log "Loading module (cfg): $name"
        insmod $name || kmsg_log "Failed to load $name"
        ;;
      "setprop")
        kmsg_log "Setting property: $name"
        setprop $name 1
        ;;
    esac
  done < $cfg_file
fi

kmsg_log "Module loading completed."

# set property even if there is no insmod config
# as property value "1" is expected in early-boot trigger
setprop vendor.all.modules.ready 1
