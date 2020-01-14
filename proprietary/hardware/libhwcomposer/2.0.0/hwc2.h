#ifndef HWC_MEDIATOR_H
#define HWC_MEDIATOR_H
#include <vector>
#include <atomic>
#include <sstream>

#include <utils/Singleton.h>
#include <utils/RefBase.h>

#ifdef USES_PQSERVICE
#include <vendor/mediatek/hardware/pq/2.0/IPictureQuality.h>
using vendor::mediatek::hardware::pq::V2_0::IPictureQuality;
#endif

#include "color.h"
#include "hwc2_api.h"
#include "display.h"
#include "utils/tools.h"

class HWCDisplay;
class IOverlayDevice;

typedef enum {
    HWC_SKIP_VALIDATE_NOT_SKIP = 0,
    HWC_SKIP_VALIDATE_SKIP = 1
} SKIP_VALI_STATE;

// Track the sequence state from validate to present
// +-----------------+------------------------------------+----------------------------+
// | State           | Define                             | Next State                 |
// +-----------------+------------------------------------+----------------------------+
// | PRESENT_DONE    | SF get release fence / Initinal    | CHECK_SKIP_VALI / VALIDATE |
// | CHECK_SKIP_VALI | Check or Check done skip validate  | VALIDATE_DONE / VALIDATE   |
// | VALIDATE        | Doing or done validate             | VALIDATE_DONE]             |
// | VALIDATE_DONE   | SF get validate result             | PRESENT                    |
// | PRESENT         | Doing or done validate             | PRESENT_DONE               |
// +-----------------+------------------------------------+----------------------------+

typedef enum {
    HWC_VALI_PRESENT_STATE_PRESENT_DONE = 0,
    HWC_VALI_PRESENT_STATE_CHECK_SKIP_VALI = 1,
    HWC_VALI_PRESENT_STATE_VALIDATE = 2,
    HWC_VALI_PRESENT_STATE_VALIDATE_DONE = 3,
    HWC_VALI_PRESENT_STATE_PRESENT = 4
} HWC_VALI_PRESENT_STATE;

class HWCBuffer : public android::LightRefBase<HWCBuffer>
{
    public:
        HWCBuffer(const uint64_t& disp_id, const int32_t& layer_id, const bool& is_ct):
            m_hnd(nullptr),
            m_prev_hnd(nullptr),
            m_release_fence_fd(-1),
            m_prev_release_fence_fd(-1),
            m_acquire_fence_fd(-1),
            m_is_ct(is_ct),
            m_disp_id(disp_id),
            m_layer_id(layer_id),
            m_buffer_changed(false),
            m_prexform_changed(false)
        { }

        ~HWCBuffer();

        void setHandle(const buffer_handle_t& hnd)
        {
            const uint64_t prev_alloc_id = m_priv_hnd.alloc_id;
            int32_t err = 0;
            if (hnd != nullptr)
                err = gralloc_extra_query(hnd, GRALLOC_EXTRA_GET_ID, &m_priv_hnd.alloc_id);

            if (err)
                HWC_LOGE("%s err(%x), (handle=%p)", __func__, err, hnd);

            if (getPrevHandle() != hnd || prev_alloc_id != m_priv_hnd.alloc_id || err)
            {
                setBufferChanged(true);
            }

            m_hnd = hnd;
        }

        buffer_handle_t getPrevHandle() const { return m_prev_hnd; }
        void setPrevHandle(const buffer_handle_t& prev_hnd) { m_prev_hnd = prev_hnd; }

        buffer_handle_t getHandle() const { return m_hnd; }

        void setReleaseFenceFd(const int32_t& fence_fd, const bool& is_disp_connected);
        int32_t getReleaseFenceFd() { return m_release_fence_fd; }

        void setPrevReleaseFenceFd(const int32_t& fence_fd, const bool& is_disp_connected);
        int32_t getPrevReleaseFenceFd() { return m_prev_release_fence_fd; }

        void setAcquireFenceFd(const int32_t& fence_fd, const bool& is_disp_connected);
        int32_t getAcquireFenceFd() { return m_acquire_fence_fd; }

        const PrivateHandle& getPrivateHandle() { return m_priv_hnd; }
        PrivateHandle& getEditablePrivateHandle() { return m_priv_hnd; }

        int32_t afterPresent(const bool& is_disp_connected, const bool& is_ct = false);

        void setBufferChanged(const bool& buf_changed) { m_buffer_changed = buf_changed; }
        bool isBufferChanged() const { return m_buffer_changed; }

        bool isPrexformChanged() const { return m_prexform_changed; }

        void setupPrivateHandle()
        {
            if (m_hnd != nullptr)
            {
                uint32_t prevPrexForm = m_priv_hnd.prexform;
                // todo: should not pass nullptr to getPrivateHandle
                (m_is_ct)? getPrivateHandleFBT(m_hnd, &m_priv_hnd) : (::getPrivateHandle(m_hnd, &m_priv_hnd));

                if (prevPrexForm != m_priv_hnd.prexform)
                {
                    m_prexform_changed = true;
                }
                else
                {
                    m_prexform_changed = false;
                }
            }
            else
            {
                // it's a dim layer?
                m_priv_hnd.format = HAL_PIXEL_FORMAT_RGBA_8888;
            }
        }
    private:
        buffer_handle_t m_hnd;
        buffer_handle_t m_prev_hnd;
        int32_t m_release_fence_fd;
        int32_t m_prev_release_fence_fd;
        int32_t m_acquire_fence_fd;
        bool m_is_ct;
        PrivateHandle m_priv_hnd;
        uint64_t m_disp_id;
        int32_t m_layer_id;
        bool m_buffer_changed;
        bool m_prexform_changed;
};

class HWCLayer : public android::LightRefBase<HWCLayer>
{
public:
    static std::atomic<int64_t> id_count;

    HWCLayer(const wp<HWCDisplay>& disp, const uint64_t& disp_id, const bool& is_ct);
    ~HWCLayer();

    uint64_t getId() const { return m_id; };

    bool isClientTarget() const { return m_is_ct; }

    wp<HWCDisplay> getDisplay() { return m_disp; }

    void validate();
    int32_t afterPresent(const bool& is_disp_connected);

    void setHwlayerType(const int32_t& hwlayer_type, const int32_t& line)
    {
        m_hwlayer_type = hwlayer_type;
        m_hwlayer_type_line = line;
    }

    void setHwlayerTypeLine(const int32_t& line) { m_hwlayer_type_line = line; }

    int32_t getHwlayerType() const { return m_hwlayer_type; }
    int32_t getHwlayerTypeLine() const { return m_hwlayer_type_line; }

    int32_t getCompositionType() const;

    void setSFCompositionType(const int32_t& sf_comp_type, const bool& call_from_sf);
    int32_t getSFCompositionType() const { return m_sf_comp_type; }
    int32_t getLastCompTypeCallFromSF() const { return m_last_comp_type_call_from_sf; }
    bool isSFCompositionTypeCallFromSF() const { return m_sf_comp_type_call_from_sf; }

    void setHandle(const buffer_handle_t& hnd);
    buffer_handle_t getHandle() { return m_hwc_buf->getHandle(); }

    const PrivateHandle& getPrivateHandle() const { return m_hwc_buf->getPrivateHandle(); }

    PrivateHandle& getEditablePrivateHandle() { return m_hwc_buf->getEditablePrivateHandle(); }

    void setReleaseFenceFd(const int32_t& fence_fd, const bool& is_disp_connected);
    int32_t getReleaseFenceFd() { return m_hwc_buf->getReleaseFenceFd(); }

    void setPrevReleaseFenceFd(const int32_t& fence_fd, const bool& is_disp_connected);
    int32_t getPrevReleaseFenceFd() { return m_hwc_buf->getPrevReleaseFenceFd(); }

    void setAcquireFenceFd(const int32_t& acquire_fence_fd, const bool& is_disp_connected);
    int32_t getAcquireFenceFd() { return m_hwc_buf->getAcquireFenceFd(); }

    void setDataspace(const int32_t& dataspace);
    int32_t getDataspace() { return m_dataspace; }

    void setDamage(const hwc_region_t& damage);
    const hwc_region_t& getDamage() { return m_damage; }

    void setBlend(const int32_t& blend);
    int32_t getBlend() { return m_blend; }

    void setDisplayFrame(const hwc_rect_t& display_frame);
    const hwc_rect_t& getDisplayFrame() const { return m_display_frame; }

    void setSourceCrop(const hwc_frect_t& source_crop);
    const hwc_frect_t& getSourceCrop() const { return m_source_crop; }

    void setZOrder(const uint32_t& z_order);
    uint32_t getZOrder() { return m_z_order; }

    void setPlaneAlpha(const float& plane_alpha);
    float getPlaneAlpha() { return m_plane_alpha; }

    void setTransform(const int32_t& transform);
    int32_t getTransform() const { return m_transform; }

    void setVisibleRegion(const hwc_region_t& visible_region);
    const hwc_region_t& getVisibleRegion() { return m_visible_region; }

    void setBufferChanged(const bool& changed) { return m_hwc_buf->setBufferChanged(changed); }
    bool isBufferChanged() const { return m_hwc_buf->isBufferChanged(); }

    void setStateChanged(const bool& changed) { m_state_changed = changed; }
    bool isStateChanged() const { return m_state_changed; }

    void setMtkFlags(const int64_t& mtk_flags) {m_mtk_flags = mtk_flags; }
    int64_t getMtkFlags() const { return m_mtk_flags; }

    void setLayerColor(const hwc_color_t& color);
    uint32_t getLayerColor() { return m_layer_color; }

    const hwc_rect_t& getMdpDstRoi() const { return m_mdp_dst_roi; }
    hwc_rect_t& editMdpDstRoi() { return m_mdp_dst_roi; }

    sp<HWCBuffer> getHwcBuffer() { return m_hwc_buf; }

    void toBeDim();

    void setupPrivateHandle()
    {
        m_hwc_buf->setupPrivateHandle();
        if (m_hwc_buf->isPrexformChanged())
            setStateChanged(true);
    }

    void setVisible(const bool& is_visible) { m_is_visible = is_visible; }
    bool isVisible() const { return m_is_visible; }

    String8 toString8();

    // return final transform rectify with prexform
    uint32_t getXform() const;
    bool needRotate() const;
    bool needScaling() const;

    void setLayerCaps(const int32_t& layer_caps) { m_layer_caps = layer_caps; }
    int32_t getLayerCaps() const { return m_layer_caps; }
private:
    int64_t m_mtk_flags;

    int64_t m_id;

    const bool m_is_ct;

    wp<HWCDisplay> m_disp;

    int32_t m_hwlayer_type;
    int32_t m_hwlayer_type_line;

    int32_t m_sf_comp_type;

    int32_t m_dataspace;

    hwc_region_t m_damage;

    int32_t m_blend;

    hwc_rect_t m_display_frame;

    hwc_frect_t m_source_crop;

    float m_plane_alpha;

    uint32_t m_z_order;

    int32_t m_transform;

    hwc_region_t m_visible_region;

    bool m_state_changed;

    PrivateHandle m_priv_hnd;

    hwc_rect_t m_mdp_dst_roi;

    uint64_t m_disp_id;

    sp<HWCBuffer> m_hwc_buf;

    bool m_is_visible;

    bool m_sf_comp_type_call_from_sf;

    int32_t m_last_comp_type_call_from_sf;

    int32_t m_layer_caps;

    uint32_t m_layer_color;
};

class HWCDisplay : public RefBase
{
public:
    HWCDisplay(const int64_t& disp_id, const int32_t& type);

    void init();

    void validate();
    void beforePresent(const int32_t num_validate_display);

    void present();

    void afterPresent();

    void clear();

    bool isConnected() const;

    bool isValidated() const;

    void getChangedCompositionTypes(
        uint32_t* out_num_elem,
        hwc2_layer_t* out_layers,
        int32_t* out_types) const;

    sp<HWCLayer> getLayer(const hwc2_layer_t& layer_id);

    const std::vector<sp<HWCLayer> >& getVisibleLayersSortedByZ();
    const std::vector<sp<HWCLayer> >& getInvisibleLayersSortedByZ();
    const std::vector<sp<HWCLayer> >& getCommittedLayers();

    sp<HWCLayer> getClientTarget();

    int32_t getWidth() const;
    int32_t getHeight() const;
    int32_t getVsyncPeriod() const;
    int32_t getDpiX() const;
    int32_t getDpiY() const;
    int32_t getSecure() const;

    void setPowerMode(const int32_t& mode);
    uint32_t getPowerMode() { return m_power_mode; }

    void setVsyncEnabled(const int32_t& enabled);

    void getType(int32_t* out_type) const;

    uint64_t getId() const { return m_disp_id; }

    int32_t createLayer(hwc2_layer_t* out_layer, const bool& is_ct);
    int32_t destroyLayer(const hwc2_layer_t& layer);

    int32_t getMirrorSrc() const { return m_mir_src; }
    void setMirrorSrc(const int64_t& disp) { m_mir_src = disp; }

    void getGlesRange(int32_t* gles_head, int32_t* gles_tail) const
    {
        *gles_head = m_gles_head;
        *gles_tail = m_gles_tail;
    }

    void setGlesRange(const int32_t& gles_head, const int32_t& gles_tail);

    void updateGlesRange();

    void acceptChanges();

    int32_t getRetireFenceFd() const {
        return m_retire_fence_fd;
    }

    void setRetireFenceFd(const int32_t& retire_fence_fd, const bool& is_disp_connected);

    void clearAllFences();

    // close the retire fence, acquire fence of output buffer, and fbt fences
    void clearDisplayFencesAndFbtFences();

    const std::vector<int32_t>& getPrevCompTypes() { return m_prev_comp_types; }

    void moveChangedCompTypes(std::vector<sp<HWCLayer> >* changed_comp_types)
    {
        // todo: move ?
        m_changed_comp_types = *changed_comp_types;
    }

    void getReleaseFenceFds(
        uint32_t* out_num_elem,
        hwc2_layer_t* out_layer,
        int32_t* out_fence_fd);

    void getClientTargetSupport(
        const uint32_t& width, const uint32_t& height,
        const int32_t& format, const int32_t& dataspace);

    void setOutbuf(const buffer_handle_t& handle, const int32_t& release_fence_fd);
    sp<HWCBuffer> getOutbuf() { return m_outbuf;}

    void setMtkFlags(const int64_t& mtk_flags) {m_mtk_flags = mtk_flags; }
    int64_t getMtkFlags() const { return m_mtk_flags; }

    void dump(String8* dump_str);

    void initPrevCompTypes();

    void buildVisibleAndInvisibleLayersSortedByZ();
    void buildCommittedLayers();

    int32_t getColorTransformHint() { return m_color_transform_hint; }
    int32_t setColorTransform(const float* matrix, const int32_t& hint);

    int32_t getColorMode() { return m_color_mode; }
    void setColorMode(const int32_t& color_mode) { m_color_mode = color_mode; }

    bool needDisableVsyncOffset() { return m_need_av_grouping; }

    void setupPrivateHandleOfLayers();

    const std::vector<sp<HWCLayer> >& getLastCommittedLayers() const { return m_last_committed_layers; }
    void setLastCommittedLayers(const std::vector<sp<HWCLayer> >& last_committed_layers) { m_last_committed_layers = last_committed_layers; }

    bool isGeometryChanged()
    {
#ifndef MTK_USER_BUILD
        ATRACE_CALL();
#endif
        const auto& committed_layers = getCommittedLayers();
        const auto& last_committed_layers = getLastCommittedLayers();
        if (committed_layers != last_committed_layers)
            return true;

        bool is_dirty = false;

        for (auto& layer: committed_layers)
        {
            is_dirty |= layer->isStateChanged();
        }
        return is_dirty;
    }

    void removePendingRemovedLayers();

    bool isValid()
    {
        if (!isConnected() || getPowerMode() == HWC2_POWER_MODE_OFF)
            return false;

        return true;
    }

    void setJobDisplayOrientation();
    bool isForceGpuCompose();

    int getPrevAvailableInputLayerNum() const { return m_prev_available_input_layer_num; }
    void setPrevAvailableInputLayerNum(int availInputNum) { m_prev_available_input_layer_num = availInputNum; }

    void setValiPresentState(HWC_VALI_PRESENT_STATE val, const int32_t& line);

    HWC_VALI_PRESENT_STATE getValiPresentState() const { return m_vali_present_state; }

    bool isVisibleLayerChanged() const { return m_is_visible_layer_changed; }
    void checkVisibleLayerChange(const std::vector<sp<HWCLayer> > &prev_visible_layers);
    void setColorTransformForJob(DispatcherJob* const job);
private:
    bool needDoAvGrouping(const int32_t num_validate_display);

    int64_t m_mtk_flags;

    int32_t m_type;
    sp<HWCBuffer> m_outbuf;

    bool m_is_validated;
    std::vector<sp<HWCLayer> > m_changed_comp_types;

    int64_t m_disp_id;

    std::map<int64_t, sp<HWCLayer> > m_layers;
    mutable Mutex m_pending_removed_layers_mutex;
    std::set<uint64_t> m_pending_removed_layers_id;
    std::vector<sp<HWCLayer> > m_visible_layers;
    std::vector<sp<HWCLayer> > m_invisible_layers;
    std::vector<sp<HWCLayer> > m_committed_layers;

    // client target
    sp<HWCLayer> m_ct;

    int32_t m_gles_head;
    int32_t m_gles_tail;
    int32_t m_retire_fence_fd;
    int32_t m_mir_src;
    std::vector<int32_t> m_prev_comp_types;
    uint32_t m_power_mode;
    int32_t m_color_transform_hint;
    int32_t m_color_mode;

    bool m_need_av_grouping;

    std::vector<sp<HWCLayer> > m_last_committed_layers;
    bool m_color_transform_ok;
    sp<ColorTransform> m_color_transform;
    CCORR_STATE m_ccorr_state;
    int m_prev_available_input_layer_num;

    HWC_VALI_PRESENT_STATE m_vali_present_state;
    bool m_is_visible_layer_changed;
};

class DisplayListener : public DisplayManager::EventListener
{
public:
    DisplayListener(
        const HWC2_PFN_HOTPLUG callback_hotplug,
        const hwc2_callback_data_t callback_hotplug_data,
        const HWC2_PFN_VSYNC callback_vsync,
        const hwc2_callback_data_t callback_vsync_data,
        const HWC2_PFN_REFRESH callback_refresh,
        const hwc2_callback_data_t callback_refresh_data);

        HWC2_PFN_HOTPLUG m_callback_hotplug;
        hwc2_callback_data_t m_callback_hotplug_data;

        HWC2_PFN_VSYNC   m_callback_vsync;
        hwc2_callback_data_t m_callback_vsync_data;

        HWC2_PFN_REFRESH m_callback_refresh;
        hwc2_callback_data_t m_callback_refresh_data;
private:
    virtual void onVSync(int dpy, nsecs_t timestamp, bool enabled);

    virtual void onPlugIn(int dpy);

    virtual void onPlugOut(int dpy);

    virtual void onHotPlugExt(int dpy, int connected);

    virtual void onRefresh(int dpy);

    virtual void onRefresh(int dpy, unsigned int type);
};

class Hrt
{
public:
    Hrt()
    {
        memset(m_layer_config_list, 0, sizeof(layer_config*) * DisplayManager::MAX_DISPLAYS);
        memset(m_layer_config_len, 0, sizeof(int) * DisplayManager::MAX_DISPLAYS);
        memset(&m_disp_layer, 0, sizeof(disp_layer_info));
    }
    ~Hrt()
    {
        for (int i = 0; i < DisplayManager::MAX_DISPLAYS; ++i)
        {
            if (m_layer_config_list[i])
                free(m_layer_config_list);
        }
    }

    void run(std::vector<sp<HWCDisplay> >& displays, const bool& is_skip_validate);

    bool isEnabled() const;

    void dump(String8* str);

private:
    void printQueryValidLayerResult();

    void modifyMdpDstRoiIfRejectedByRpo(const std::vector<sp<HWCDisplay> >& displays);

    void fillLayerConfigList(const std::vector<sp<HWCDisplay> >& displays);

    void fillDispLayer(const std::vector<sp<HWCDisplay> >& displays);

    void fillLayerInfoOfDispatcherJob(const std::vector<sp<HWCDisplay> >& displays);

    void setCompType(const std::vector<sp<HWCDisplay> >& displays);

    layer_config* m_layer_config_list[DisplayManager::MAX_DISPLAYS];
    int m_layer_config_len[DisplayManager::MAX_DISPLAYS];

    disp_layer_info m_disp_layer;

    std::stringstream m_hrt_result;
};

class HWCMediator : public HWC2Api, public android::Singleton<HWCMediator>
{
public:
    HWCMediator();
    ~HWCMediator();

    sp<HWCDisplay> getHWCDisplay(const uint64_t& disp_id)
    {
        return disp_id < m_displays.size() ? m_displays[disp_id] : nullptr;
    }

    void addHWCDisplay(const sp<HWCDisplay>& display);
    void deleteHWCDisplay(const sp<HWCDisplay>& display);
    DbgLogger& editSetBufFromSfLog() { return m_set_buf_from_sf_log; }
    DbgLogger& editSetCompFromSfLog() { return m_set_comp_from_sf_log; }
    sp<IOverlayDevice> getOvlDevice(const uint64_t& dpy) { return m_disp_devs[dpy]; }

    void addDriverRefreshCount()
    {
        AutoMutex l(m_driver_refresh_count_mutex);
        ++m_driver_refresh_count;
    }
    void decDriverRefreshCount()
    {
        AutoMutex l(m_driver_refresh_count_mutex);
        if (m_driver_refresh_count > 0)
            --m_driver_refresh_count;
    }
    int getDriverRefreshCount() const
    {
        AutoMutex l(m_driver_refresh_count_mutex);
        return m_driver_refresh_count;
    }
private:
    void updateGlesRangeForAllDisplays();

    void unpackageMtkData(const HwcValidateData& val_data);

    void adjustVsyncOffset();

/*-------------------------------------------------------------------------*/
/* Skip Validate */
    bool checkSkipValidate();

    int getValidDisplayNum();

    void buildVisibleAndInvisibleLayerForAllDisplay();

    void prepareForValidation();

    void validate();

    void countdowmSkipValiRelatedNumber();

    void setValiPresentStateOfAllDisplay(const HWC_VALI_PRESENT_STATE& val, const int32_t& line);

    SKIP_VALI_STATE getNeedValidate() const { return m_need_validate; }
    void setNeedValidate(SKIP_VALI_STATE val) { m_need_validate = val; }

    int32_t getLastSFValidateNum() const { return m_last_SF_validate_num; }
    void setLastSFValidateNum(int32_t val) { m_last_SF_validate_num = val; }
/*-------------------------------------------------------------------------*/

/*-------------------------------------------------------------------------*/
/* Refresh event lock */
    bool isAllDisplayPresentDone();
    void lockRefreshThread(hwc2_display_t display);
    void unlockRefreshThread(hwc2_display_t display);
    bool isValidated() const { return m_is_valied; }
    void setValidated(bool val) { m_is_valied = val; }
public:
    void lockRefreshVali() { m_refresh_vali_lock.lock(); }
    void unlockRefreshVali() { m_refresh_vali_lock.unlock(); }
/*-------------------------------------------------------------------------*/

    std::vector<sp<HWCDisplay> > m_displays;

    Hrt m_hrt;

    SKIP_VALI_STATE m_need_validate;
    int32_t m_last_SF_validate_num;

    int32_t m_validate_seq;
    int32_t m_present_seq;

    bool m_vsync_offset_state;

    DbgLogger m_set_buf_from_sf_log;
    DbgLogger m_set_comp_from_sf_log;
    std::vector<sp<IOverlayDevice> > m_disp_devs;

    std::vector<int32_t> m_capabilities;

    int m_driver_refresh_count;
    mutable Mutex m_driver_refresh_count_mutex;

/*-------------------------------------------------------------------------*/
/* Refresh event lock */
    bool m_is_valied;
    mutable Mutex m_refresh_vali_lock;
/*-------------------------------------------------------------------------*/

public:
    void open(/*hwc_private_device_t* device*/);

    void close(/*hwc_private_device_t* device*/);

    void getCapabilities(
        uint32_t* out_count,
        int32_t* /*hwc2_capability_t*/ out_capabilities);

    bool hasCapabilities(int32_t capabilities);

    void createExternalDisplay();
    void destroyExternalDisplay();

    /* Device functions */
    int32_t /*hwc2_error_t*/ deviceCreateVirtualDisplay(
        hwc2_device_t* device,
        uint32_t width,
        uint32_t height,
        int32_t* /*android_pixel_format_t*/ format,
        hwc2_display_t* outDisplay);

    int32_t /*hwc2_error_t*/ deviceDestroyVirtualDisplay(
        hwc2_device_t* device,
        hwc2_display_t display);

    void deviceDump(hwc2_device_t* device, uint32_t* outSize, char* outBuffer);

    uint32_t deviceGetMaxVirtualDisplayCount(hwc2_device_t* device);

    int32_t /*hwc2_error_t*/ deviceRegisterCallback(
        hwc2_device_t* device,
        int32_t /*hwc2_callback_descriptor_t*/ descriptor,
        hwc2_callback_data_t callbackData,
        hwc2_function_pointer_t pointer);

    /* Display functions */
    int32_t /*hwc2_error_t*/ displayAcceptChanges(
        hwc2_device_t* device,
        hwc2_display_t display);

    int32_t /*hwc2_error_t*/ displayCreateLayer(
        hwc2_device_t* device,
        hwc2_display_t disply,
        hwc2_layer_t* outLayer);

    int32_t /*hwc2_error_t*/ displayDestroyLayer(
        hwc2_device_t* device,
        hwc2_display_t display,
        hwc2_layer_t layer);

    int32_t /*hwc2_error_t*/ displayGetActiveConfig(
        hwc2_device_t* device,
        hwc2_display_t display,
        hwc2_config_t* out_config);

    int32_t /*hwc2_error_t*/ displayGetChangedCompositionTypes(
        hwc2_device_t* device,
        hwc2_display_t display,
        uint32_t* out_num_elem,
        hwc2_layer_t* out_layers,
        int32_t* /*hwc2_composition_t*/ out_types);

    int32_t /*hwc2_error_t*/ displayGetClientTargetSupport(
        hwc2_device_t* device,
        hwc2_display_t display,
        uint32_t width,
        uint32_t height,
        int32_t /*android_pixel_format_t*/ format,
        int32_t /*android_dataspace_t*/ dataspace);

    int32_t /*hwc2_error_t*/ displayGetColorMode(
        hwc2_device_t* device,
        hwc2_display_t display,
        uint32_t* out_num_modes,
        int32_t* out_modes);

    int32_t /*hwc2_error_t*/ displayGetAttribute(
        hwc2_device_t* device,
        hwc2_display_t display,
        hwc2_config_t config,
        int32_t /*hwc2_attribute_t*/ attribute,
        int32_t* out_value);

    int32_t /*hwc2_error_t*/ displayGetConfigs(
        hwc2_device_t* device,
        hwc2_display_t display,
        uint32_t* out_num_configs,
        hwc2_config_t* out_configs);

    int32_t /*hwc2_error_t*/ displayGetName(
        hwc2_device_t* device,
        hwc2_display_t display,
        uint32_t* out_lens,
        char* out_name);

    int32_t /*hwc2_error_t*/ displayGetRequests(
        hwc2_device_t* device,
        hwc2_display_t display,
        int32_t* /*hwc2_display_request_t*/ out_display_requests,
        uint32_t* out_num_elem,
        hwc2_layer_t* out_layer,
        int32_t* /*hwc2_layer_request_t*/ out_layer_requests);

    int32_t /*hwc2_error_t*/ displayGetType(
        hwc2_device_t* device,
        hwc2_display_t display,
        int32_t* /*hwc2_display_type_t*/ out_type);

    int32_t /*hwc2_error_t*/ displayGetDozeSupport(
        hwc2_device_t* device,
        hwc2_display_t display,
        int32_t* out_support);

    int32_t /*hwc2_error_t*/ displayGetHdrCapability(
        hwc2_device_t* device,
        hwc2_display_t display,
        uint32_t* out_num_types,
        int32_t* /*android_hdr_t*/ out_types,
        float* out_max_luminance,
        float* out_max_avg_luminance,
        float* out_min_luminance);

    int32_t /*hwc2_error_t*/ displayGetReleaseFence(
        hwc2_device_t* device,
        hwc2_display_t display,
        uint32_t* out_num_elem,
        hwc2_layer_t* out_layer,
        int32_t* out_fence);

    int32_t /*hwc2_error_t*/ displayPresent(
        hwc2_device_t* device,
        hwc2_display_t display,
        int32_t* out_retire_fence);

    int32_t /*hwc2_error_t*/ displaySetActiveConfig(
        hwc2_device_t* device,
        hwc2_display_t display,
        hwc2_config_t config_id);

    int32_t /*hwc2_error_t*/ displaySetClientTarget(
        hwc2_device_t* device,
        hwc2_display_t display,
        buffer_handle_t handle,
        int32_t acquire_fence,
        int32_t dataspace,
        hwc_region_t damage);

    int32_t /*hwc2_error_t*/ displaySetColorMode(
        hwc2_device_t* device,
        hwc2_display_t display, int32_t mode);

    int32_t /*hwc2_error_t*/ displaySetColorTransform(
        hwc2_device_t* device,
        hwc2_display_t display,
        const float* matrix,
        int32_t /*android_color_transform_t*/ hint);

    int32_t /*hwc2_error_t*/ displaySetOutputBuffer(
        hwc2_device_t* device,
        hwc2_display_t display,
        buffer_handle_t buffer,
        int32_t releaseFence);

    int32_t /*hwc2_error_t*/ displaySetPowerMode(
        hwc2_device_t* device,
        hwc2_display_t display,
        int32_t /*hwc2_power_mode_t*/ mode);

    int32_t /*hwc2_error_t*/ displaySetVsyncEnabled(
        hwc2_device_t* device,
        hwc2_display_t display,
        int32_t /*hwc2_vsync_t*/ enabled);

    int32_t /*hwc2_error_t*/ displayValidateDisplay(
        hwc2_device_t* device,
        hwc2_display_t display,
        uint32_t* outNumTypes,
        uint32_t* outNumRequests);

    /* Layer functions */
    int32_t /*hwc2_error_t*/ layerSetCursorPosition(
        hwc2_device_t* device,
        hwc2_display_t display,
        hwc2_layer_t layer,
        int32_t x,
        int32_t y);

    int32_t /*hwc2_error_t*/ layerSetBuffer(
        hwc2_device_t* device,
        hwc2_display_t display,
        hwc2_layer_t layer,
        buffer_handle_t buffer,
        int32_t acquireFence);

    int32_t /*hwc2_error_t*/ layerSetSurfaceDamage(
        hwc2_device_t* device,
        hwc2_display_t display,
        hwc2_layer_t layer,
        hwc_region_t damage);

    /* Layer state functions */
    int32_t /*hwc2_error_t*/ layerStateSetBlendMode(
        hwc2_device_t* device,
        hwc2_display_t display,
        hwc2_layer_t layer,
        int32_t /*hwc2_blend_mode_t*/ mode);

    int32_t /*hwc2_error_t*/ layerStateSetColor(
        hwc2_device_t* device,
        hwc2_display_t display,
        hwc2_layer_t layer,
        hwc_color_t color);

    int32_t /*hwc2_error_t*/ layerStateSetCompositionType(
        hwc2_device_t* device,
        hwc2_display_t display,
        hwc2_layer_t layer,
        int32_t /*hwc2_composition_t*/ type);

    int32_t /*hwc2_error_t*/ layerStateSetDataSpace(
        hwc2_device_t* device,
        hwc2_display_t display,
        hwc2_layer_t layer,
        int32_t /*android_dataspace_t*/ dataspace);

    int32_t /*hwc2_error_t*/ layerStateSetDisplayFrame(
        hwc2_device_t* device,
        hwc2_display_t display,
        hwc2_layer_t layer,
        hwc_rect_t frame);

    int32_t /*hwc2_error_t*/ layerStateSetPlaneAlpha(
        hwc2_device_t* device,
        hwc2_display_t display,
        hwc2_layer_t layer,
        float alpha);

    int32_t /*hwc2_error_t*/ layerStateSetSidebandStream(
        hwc2_device_t* device,
        hwc2_display_t display,
        hwc2_layer_t layer,
        const native_handle_t* stream);

    int32_t /*hwc2_error_t*/ layerStateSetSourceCrop(
        hwc2_device_t* device,
        hwc2_display_t display,
        hwc2_layer_t layer,
        hwc_frect_t crop);

    int32_t /*hwc2_error_t*/ layerStateSetTransform(
        hwc2_device_t* device,
        hwc2_display_t display,
        hwc2_layer_t layer,
        int32_t /*hwc_transform_t*/ transform);

    int32_t /*hwc2_error_t*/ layerStateSetVisibleRegion(
        hwc2_device_t* device,
        hwc2_display_t display,
        hwc2_layer_t layer,
        hwc_region_t visible);

    int32_t /*hwc2_error_t*/ layerStateSetZOrder(
        hwc2_device_t* device,
        hwc2_display_t display,
        hwc2_layer_t layer,
        uint32_t z);

private:
    bool m_is_init_disp_manager;

    HWC2_PFN_HOTPLUG m_callback_hotplug;
    hwc2_callback_data_t m_callback_hotplug_data;

    HWC2_PFN_VSYNC   m_callback_vsync;
    hwc2_callback_data_t m_callback_vsync_data;

    HWC2_PFN_REFRESH   m_callback_refresh;
    hwc2_callback_data_t m_callback_refresh_data;
};

#endif // HWC_MEDIATOR_H
