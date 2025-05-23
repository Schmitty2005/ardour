/*
 * Copyright (C) 2019-2023 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef _ardour_vst3_plugin_h_
#define _ardour_vst3_plugin_h_

#include <map>
#include <set>
#include <vector>

#include <optional>
#include <glibmm/threads.h>

#include "pbd/search_path.h"
#include "pbd/signals.h"

#include "ardour/plugin.h"
#include "ardour/vst3_host.h"

namespace ARDOUR
{
class VST3PluginModule;
class AutomationList;
}

#if defined(__clang__)
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wnon-virtual-dtor"
# pragma clang diagnostic ignored "-Wdelete-non-virtual-dtor"
# pragma clang diagnostic ignored "-Wdelete-non-abstract-non-virtual-dtor"
#elif __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 7)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
# pragma GCC diagnostic ignored "-Wdelete-non-virtual-dtor"
#endif

namespace Steinberg {
/* VST3 hosted Plugin abstraction Implementation
 *
 * For convenience this is placed in the Steinberg namespace.
 * Ardour::VST3Plugin has-a VST3PI (not is-a).
 */
class LIBARDOUR_API VST3PI
	: public Vst::IComponentHandler
	, public Vst::IComponentHandler2
	, public Vst::IUnitHandler
	, public IPlugFrame
	, public Presonus::IContextInfoProvider3
{
public:
	VST3PI (std::shared_ptr<ARDOUR::VST3PluginModule> m, std::string unique_id);
	virtual ~VST3PI ();

	/* IComponentHandler */
	tresult PLUGIN_API beginEdit (Vst::ParamID id) SMTG_OVERRIDE;
	tresult PLUGIN_API performEdit (Vst::ParamID id, Vst::ParamValue value) SMTG_OVERRIDE;
	tresult PLUGIN_API endEdit (Vst::ParamID id) SMTG_OVERRIDE;
	tresult PLUGIN_API restartComponent (int32 flags) SMTG_OVERRIDE;

	/* IComponentHandler2 */
	tresult PLUGIN_API setDirty (TBool state) SMTG_OVERRIDE;
	tresult PLUGIN_API requestOpenEditor (FIDString name) SMTG_OVERRIDE;
	tresult PLUGIN_API startGroupEdit () SMTG_OVERRIDE;
	tresult PLUGIN_API finishGroupEdit () SMTG_OVERRIDE;

	/* IPlugFrame */
	tresult PLUGIN_API resizeView (IPlugView* view, ViewRect* newSize) SMTG_OVERRIDE;

	/* IUnitHandler API */
	tresult PLUGIN_API notifyUnitSelection (Vst::UnitID) SMTG_OVERRIDE;
	tresult PLUGIN_API notifyProgramListChange (Vst::ProgramListID, int32) SMTG_OVERRIDE;

	/* IContextInfoProvider3 API */
	tresult PLUGIN_API getContextInfoValue (int32&, FIDString) SMTG_OVERRIDE;
	tresult PLUGIN_API getContextInfoString (Vst::TChar*, int32, FIDString) SMTG_OVERRIDE;
	tresult PLUGIN_API getContextInfoValue (double&, FIDString) SMTG_OVERRIDE;
	tresult PLUGIN_API setContextInfoValue (FIDString, double) SMTG_OVERRIDE;
	tresult PLUGIN_API setContextInfoValue (FIDString, int32) SMTG_OVERRIDE;
	tresult PLUGIN_API setContextInfoString (FIDString, Vst::TChar*) SMTG_OVERRIDE;
	tresult PLUGIN_API beginEditContextInfoValue (FIDString) SMTG_OVERRIDE;
	tresult PLUGIN_API endEditContextInfoValue (FIDString) SMTG_OVERRIDE;

	/* GUI */
	bool       has_editor () const;
	IPlugView* view ();
	void       close_view ();
	void       update_contoller_param ();
	PBD::Signal<void(int, int)> OnResizeView;

	tresult PLUGIN_API queryInterface (const TUID _iid, void** obj) SMTG_OVERRIDE;
	uint32  PLUGIN_API addRef () SMTG_OVERRIDE { return 1; }
	uint32  PLUGIN_API release () SMTG_OVERRIDE { return 1; }

	FUID const& fuid () const { return _fuid; }

	/* Ardour Preset Helpers */
	IPtr<Vst::IUnitInfo> unit_info ();
	Vst::ParameterInfo const& program_change_port () const { return _program_change_port; }

	void set_n_factory_presets (size_t n) { _n_factory_presets = n; }
	size_t n_factory_presets () const { return _n_factory_presets; }

	/* API for Ardour -- Ports */
	uint32_t designated_bypass_port ()         const { return _port_id_bypass; }
	uint32_t parameter_count ()                const { return _ctrl_params.size (); }
	bool parameter_is_automatable (uint32_t p) const { return _ctrl_params[p].automatable; }
	bool parameter_is_readonly (uint32_t p)    const { return _ctrl_params[p].read_only; }
	std::string parameter_label (uint32_t p)   const { return _ctrl_params[p].label; }

	float       default_value (uint32_t p) const;
	void        get_parameter_descriptor (uint32_t, ARDOUR::ParameterDescriptor&) const;
	std::string print_parameter (uint32_t p) const;
	std::string print_parameter (Vst::ParamID, Vst::ParamValue) const;
	bool        set_program (int p, int32 sample_off);

	bool subscribe_to_automation_changes () const;
	void automation_state_changed (uint32_t, ARDOUR::AutoState, std::weak_ptr<ARDOUR::AutomationList>);

	ARDOUR::Plugin::IOPortDescription describe_io_port (ARDOUR::DataType dt, bool input, uint32_t id) const;

	uint32_t n_audio_inputs (bool with_aux = true) const;
	uint32_t n_audio_outputs (bool with_aux = true) const;

	uint32_t n_audio_aux_in () const { return _n_aux_inputs; }
	uint32_t n_audio_aux_out () const { return _n_aux_outputs; }

	struct AudioBusInfo {
		AudioBusInfo (Vst::BusType t, int32_t c, bool a) : type (t), n_chn (c), n_used_chn (c), dflt (a) {}
		AudioBusInfo () : type (Vst::kMain), n_chn (0), n_used_chn (0) {}
		Vst::BusType type;
		int32_t      n_chn;
		int32_t      n_used_chn;
		bool         dflt; // kDefaultActive
	};

	std::map<int, AudioBusInfo> const& bus_info_in () const { return _bus_info_in; }
	std::map<int, AudioBusInfo> const& bus_info_out () const { return _bus_info_out; }

	/* MIDI/Event interface */
	void cycle_start ();
	void add_event (Evoral::Event<samplepos_t> const&, int32_t bus);
	void vst3_to_midi_buffers (ARDOUR::BufferSet&, ARDOUR::ChanMapping const&);

	uint32_t n_midi_inputs () const;
	uint32_t n_midi_outputs () const;

	/* API for Ardour -- Parameters */
	bool         try_set_parameter_by_id (Vst::ParamID id, float value);
	void         set_parameter (uint32_t p, float value, int32 sample_off, bool to_list = true, bool force = false);
	float        get_parameter (uint32_t p) const;
	std::string  format_parameter (uint32_t p) const;
	Vst::ParamID index_to_id (uint32_t) const;

	Glib::Threads::Mutex& process_lock () { return _process_lock; }
	bool& component_is_synced () { return _restart_component_is_synced; }


	enum ParameterChange { BeginGesture,
	                       EndGesture,
	                       ValueChange,
	                       InternalChange,
	                       PresetChange,
	                       ParamValueChanged
	                     };

	PBD::Signal<void(ParameterChange, uint32_t, float)> OnParameterChange;

	/* API for Ardour -- Setup/Processing */
	uint32_t plugin_latency ();
	uint32_t plugin_tailtime ();
	bool     set_block_size (int32_t);
	bool     activate ();
	bool     deactivate ();
	bool     active () const { return _is_processing; }
	bool     is_loading_state () const { return _is_loading_state; }

	/* State */
	bool save_state (RAMStream& stream);
	bool load_state (RAMStream& stream);

	Vst::ProcessContext& context ()
	{
		return _context;
	}

	void set_owner (ARDOUR::SessionObject* o);
	void set_non_realtime (bool);

	void enable_io (std::vector<bool> const&, std::vector<bool> const&);

	void process (float** ins, float** outs, uint32_t n_samples);

	/* PSL Extension */
	Vst::IEditController* controller () const
	{
		return _controller;
	}
	bool add_slave (Vst::IEditController*, bool);
	bool remove_slave (Vst::IEditController*);

	class RouteProcessorChangeBlock
	{
		public:
			RouteProcessorChangeBlock (VST3PI* p) : _impl (p)
		{
			_impl->block_notifications ();
		}

		~RouteProcessorChangeBlock ()
		{
			_impl->resume_notifications ();
		}
		private:
			VST3PI* _impl;
	};

private:
	/* prevent copy construction */
	VST3PI (const VST3PI&);

	void terminate ();

	IPlugView* try_create_view () const;

	bool connect_components ();
	bool disconnect_components ();

	bool  update_processor ();
	int32 count_channels (Vst::MediaType, Vst::BusDirection, Vst::BusType);

	bool evoral_to_vst3 (Vst::Event&, Evoral::Event<samplepos_t> const&, int32_t);

	void update_shadow_data ();
	bool synchronize_states ();

	void set_parameter_by_id (Vst::ParamID id, float value, int32 sample_off);
	void set_parameter_internal (Vst::ParamID id, float value, int32 sample_off);

	void set_event_bus_state (bool enabled);

	bool midi_controller (int32_t, int16_t, Vst::CtrlNumber, Vst::ParamID& id);
	bool live_midi_cc (int32_t, int16_t, Vst::CtrlNumber);

	bool setup_info_listener ();
	void stripable_property_changed (PBD::PropertyChange const&);

	bool setup_psl_info_handler ();
	void psl_subscribe_to (std::shared_ptr<ARDOUR::AutomationControl>, FIDString);
	void psl_stripable_property_changed (PBD::PropertyChange const&);

	void block_notifications ();
	void resume_notifications ();
	void send_processors_changed (ARDOUR::RouteProcessorChange const&);

	void forward_signal (Presonus::IContextInfoHandler2*, FIDString) const;

	std::shared_ptr<ARDOUR::VST3PluginModule> _module;

	std::shared_ptr<ConnectionProxy> _component_cproxy;
	std::shared_ptr<ConnectionProxy> _controller_cproxy;

	FUID                  _fuid;
	Vst::IComponent*      _component;
	Vst::IEditController* _controller;
	IPlugView*            _view;

	IPtr<Vst::IAudioProcessor> _processor;
	Vst::ProcessContext        _context;
	Glib::Threads::Mutex       _process_lock;

	/* Parameters */
	Vst3ParameterChanges _input_param_changes;
	Vst3ParameterChanges _output_param_changes;

	Vst3EventList _input_events;
	Vst3EventList _output_events;

	/* state */
	bool    _is_loading_state;
	bool    _is_processing;
	int32_t _block_size;
	bool    _process_offline;

	/* ports */
	struct Param {
		uint32_t    id;
		std::string label;
		std::string unit;
		int32_t     steps; // 1: toggle
		double      normal;
		bool        is_enum;
		bool        read_only;
		bool        automatable;
	};

	uint32_t                         _port_id_bypass;
	Vst::ParameterInfo               _program_change_port;
	std::vector<Param>               _ctrl_params;
	std::map<Vst::ParamID, uint32_t> _ctrl_id_index;
	std::map<uint32_t, Vst::ParamID> _ctrl_index_id;
	std::vector<float>               _shadow_data;
	mutable std::vector<bool>        _update_ctrl;

	std::vector<ARDOUR::Plugin::IOPortDescription> _io_name[Vst::kNumMediaTypes][2];

	std::vector<bool> _enabled_audio_in;
	std::vector<bool> _enabled_audio_out;

	/* PSL extensions, control protocol */
	ARDOUR::SessionObject*      _owner;
	PBD::ScopedConnectionList   _strip_connections;
	PBD::ScopedConnectionList   _ac_connection_list;
	std::set<Evoral::Parameter> _ac_subscriptions;
	bool                        _add_to_selection;

	std::optional<uint32_t> _plugin_latency;
	std::optional<uint32_t> _plugin_tail;

	int _n_bus_in;
	int _n_bus_out;

	std::vector<Vst::AudioBusBuffers> _busbuf_in;
	std::vector<Vst::AudioBusBuffers> _busbuf_out;

	/* cache channels/bus Vst::AudioBusBuffers::numChannels */
	std::map<int, int> _n_buschn_in;
	std::map<int, int> _n_buschn_out;

	std::map<int, AudioBusInfo> _bus_info_in;
	std::map<int, AudioBusInfo> _bus_info_out;

	int _n_inputs;
	int _n_outputs;
	int _n_aux_inputs;
	int _n_aux_outputs;
	int _n_midi_inputs;
	int _n_midi_outputs;
	int _n_factory_presets;

	mutable std::atomic<int>     _block_rpc;
	ARDOUR::RouteProcessorChange _rpc_queue;

	/* work around UADx plugin crash */
	bool _no_kMono;
	/* work around yabridge threading */
	bool _restart_component_is_synced;
	/* work around PSL calls during set_owner,
	 * while the route holds a processor lock
	 */
	std::atomic<bool> _in_set_owner;
};

} // namespace Steinberg

namespace ARDOUR {

class LIBARDOUR_API VST3Plugin : public ARDOUR::Plugin
{
public:
	VST3Plugin (AudioEngine&, Session&, Steinberg::VST3PI*);
	VST3Plugin (const VST3Plugin&);
	~VST3Plugin ();

	std::string unique_id () const { return get_info ()->unique_id; }
	const char* name ()      const { return get_info ()->name.c_str (); }
	const char* label ()     const { return get_info ()->name.c_str (); }
	const char* maker ()     const { return get_info ()->creator.c_str (); }

	uint32_t parameter_count () const;
	float    default_value (uint32_t port);
	void     set_parameter (uint32_t port, float val, sampleoffset_t when);
	float    get_parameter (uint32_t port) const;
	int      get_parameter_descriptor (uint32_t which, ParameterDescriptor&) const;
	uint32_t nth_parameter (uint32_t port, bool& ok) const;
	bool     print_parameter (uint32_t, std::string&) const;

	bool parameter_is_audio (uint32_t)   const { return false; }
	bool parameter_is_control (uint32_t) const { return true; }

	bool parameter_is_input (uint32_t) const;
	bool parameter_is_output (uint32_t) const;

	uint32_t designated_bypass_port ();

	std::set<Evoral::Parameter> automatable () const;
	std::string                 describe_parameter (Evoral::Parameter);
	IOPortDescription           describe_io_port (DataType dt, bool input, uint32_t id) const;
	PluginOutputConfiguration   possible_output () const;

	void set_automation_control (uint32_t, std::shared_ptr<ARDOUR::AutomationControl>);

	std::string state_node_name () const
	{
		return "vst3";
	}

	void add_state (XMLNode*) const;
	int  set_state (const XMLNode&, int version);

	bool        load_preset (PresetRecord);
	std::string do_save_preset (std::string);
	void        do_remove_preset (std::string);

	void activate ()
	{
		_plug->activate ();
	}

	void deactivate ()
	{
		_plug->deactivate ();
	}

	int set_block_size (pframes_t);

	void set_owner (ARDOUR::SessionObject* o);
	void set_non_realtime (bool);

	void add_slave (std::shared_ptr<Plugin>, bool);
	void remove_slave (std::shared_ptr<Plugin>);

	int connect_and_run (BufferSet&  bufs,
	                     samplepos_t start, samplepos_t end, double speed,
	                     ChanMapping const& in, ChanMapping const& out,
	                     pframes_t nframes, samplecnt_t offset);

	bool                  has_editor () const;
	Steinberg::IPlugView* view ();
	void                  close_view ();
	void                  update_contoller_param ();

	PBD::Signal<void(int, int)> OnResizeView;

private:
	samplecnt_t plugin_latency () const;
	samplecnt_t plugin_tailtime () const;
	void        init ();
	void        find_presets ();
	void        forward_resize_view (int w, int h);
	void        parameter_change_handler (Steinberg::VST3PI::ParameterChange, uint32_t, float);

	Steinberg::VST3PI* _plug;

	PBD::ScopedConnectionList _connections;

	std::map<std::string, std::string> _preset_uri_map;

	std::vector<bool> _connected_inputs;
	std::vector<bool> _connected_outputs;

	struct PV {
		PV () : port (0), val (0) {}
		PV (uint32_t p, float v) : port (p), val (v) {}
		uint32_t port;
		float    val;
	};

	PBD::RingBufferNPT<PV> _parameter_queue;
};

/* ****************************************************************************/

class LIBARDOUR_API VST3PluginInfo : public PluginInfo, public std::enable_shared_from_this<ARDOUR::VST3PluginInfo>
{
public:
	VST3PluginInfo ();
	~VST3PluginInfo () {}

	PluginPtr                         load (Session& session);
	std::vector<Plugin::PresetRecord> get_presets (bool user_only) const;
	bool                              is_instrument () const;
	PBD::Searchpath                   preset_search_path () const;

	std::optional<bool>             has_editor;

	std::shared_ptr<VST3PluginModule> m;
};

#if defined(__clang__)
#    pragma clang diagnostic pop
#elif __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)
#    pragma GCC diagnostic pop
#endif

} // namespace ARDOUR
#endif
