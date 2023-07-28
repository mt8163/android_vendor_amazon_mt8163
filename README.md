## Building LineageOS 20.0

Building LineageOS 20.0 from source requires some familiarity with Linux and the command line. Follow these steps to get started:

# Step 1: Install Google's Repo tool

#### Before you begin, make sure you have the necessary tools:
```bash
sudo apt-get install bc bison build-essential ccache curl flex g++-multilib gcc-multilib git gnupg gperf imagemagick lib32ncurses5-dev lib32readline-dev lib32z1-dev liblz4-tool libncurses5 libncurses5-dev libsdl1.2-dev libssl-dev libxml2 libxml2-utils lzop pngcrush rsync schedtool squashfs-tools xsltproc zip zlib1g-dev
```

# Now, install Google's repo tool:
```bash
mkdir -p ~/bin
echo 'PATH=~/bin:$PATH' >> ~/.bashrc
source ~/.bashrc
curl https://storage.googleapis.com/git-repo-downloads/repo > ~/bin/repo
chmod a+x ~/bin/repo
```

# Step 2: Download LineageOS 20.0 source code

#### Create a directory for LineageOS source code and navigate to it
```bash
mkdir -p ~/android/lineage
cd ~/android/lineage
```

#### Initialize repo tool with LineageOS 20.1 source
```bash
repo init -u git://github.com/mt8163/android.git -b lineage-20.0
git clone https://github.com/mt8163/local_manifests -b lineage-20.0 .repo/local_manifests
repo sync
```

# Step 3: Build LineageOS

#### Make sure you have `adb` and `fastboot` installed and in your PATH. If not, follow the instructions here: [ADB and Fastboot installation guide](https://wiki.lineageos.org/devices/bacon/build#build-lineageos-and-lineageos-recovery)
```bash
source build/envsetup.sh
brunch lineage_karnak-userdebug
```
#### Improving build times with ccache

#### To speed up future builds, you can enable ccache support. Run the following command before each build:
```bash
export USE_CCACHE=1
```
#### To persistently enable ccache and set the maximum disk space it can use, add the following lines to your `~/.bashrc` file:
```bash
export USE_CCACHE=1
ccache -M 50G # Set cache size to 50GB (adjust as needed)
```
#### Using ccache can significantly reduce build times, so consider allocating enough disk space for caching.

>* Note: The above instructions are meant for LineageOS 20.0. Make sure to use the appropriate branch and repository URLs for other versions.

#### For more details and troubleshooting, refer to the official LineageOS documentation: [LineageOS Wiki](https://wiki.lineageos.org/devices/bacon/build#turn-on-caching-to-speed-up-build)
