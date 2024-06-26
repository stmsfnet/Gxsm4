/* -*- Mode: C++; indent-tabs-mode: nil; c-basic-offset: 8 c-style: "K&R" -*- */

/* Gnome gxsm - Gnome X Scanning Microscopy
 * universal STM/AFM/SARLS/SPALEED/... controlling and
 * data analysis software
 *
 * Gxsm Plugin Name: spm_scancontrol.h
 * ========================================
 * 
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Authors: Percy Zahl <zahl@fkp.uni-hannover.de>
 * additional features: Andreas Klust <klust@fkp.uni-hannover.de>
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __RPSPMC_PACPLL_H
#define __RPSPMC_PACPLL_H

#include <config.h>
#include "plugin.h"

#include "unit.h"
#include "pcs.h"
#include "xsmtypes.h"
#include "glbvars.h"
#include "action_id.h"

#include "core-source/app_profile.h"
#include "rpspmc_hwi_structs.h"

#include "json_talk.h"
#include "rpspmc_stream.h"


#define MAX_PROGRAM_VECTORS 16
#define i_X 0
#define i_Y 1
#define i_Z 2

// forward defs
extern PACPLL_parameters pacpll_parameters;
extern PACPLL_signals pacpll_signals;
extern SPMC_parameters spmc_parameters;
extern SPMC_signals spmc_signals;
extern JSON_parameter PACPLL_JSON_parameters[];
extern JSON_signal PACPLL_JSON_signals[];

class RPSPMC_Control;
class RPspmc_pacpll;

extern RPSPMC_Control *RPSPMC_ControlClass;

extern "C++" {
        extern RPspmc_pacpll *rpspmc_pacpll;
        extern GxsmPlugin rpspmc_pacpll_hwi_pi;
}

#define DMA_SIZE         0x40000            // 20bit count of 32bit words ==> 1MB DMA Block:  2 x 0x80000 bytes
#define EXPAND_MULTIPLES 32

// GUI builder helper
class GUI_Builder : public BuildParam{
public:
        GUI_Builder (GtkWidget *build_grid=NULL, GSList *ec_list_start=NULL, GSList *ec_remote_list=NULL) :
                BuildParam (build_grid, ec_list_start, ec_remote_list) {
                wid = NULL;
                config_checkbutton_list = NULL;
                scan_freeze_widget_list = NULL;
        };

        void start_notebook_tab (GtkWidget *notebook, const gchar *name, const gchar *settings_name,
                                 GSettings *settings) {
                new_grid (); tg=grid;
                gtk_widget_set_hexpand (grid, TRUE);
                gtk_widget_set_vexpand (grid, TRUE);
                grid_add_check_button("Configuration: Enable This");
                g_object_set_data (G_OBJECT (button), "TabGrid", grid);
                config_checkbutton_list = g_slist_append (config_checkbutton_list, button);
                configure_list = g_slist_prepend (configure_list, button);

                new_line ();
                
		page = gtk_label_new (name);
		gtk_widget_show (page);
		gtk_notebook_append_page (GTK_NOTEBOOK (notebook), grid, page);

                g_settings_bind (settings, settings_name,
                                 G_OBJECT (button), "active",
                                 G_SETTINGS_BIND_DEFAULT);
        };

        void notebook_tab_show_all () {
                gtk_widget_show (tg);
        };

        GtkWidget* grid_add_mixer_options (gint channel, gint preset, gpointer ref);
        GtkWidget* grid_add_scan_input_signal_options (gint channel, gint preset, gpointer ref);
        GtkWidget* grid_add_probe_source_signal_options (gint channel, gint preset, gpointer ref);

        GtkWidget* grid_add_probe_status (const gchar *status_label);
        void grid_add_probe_controls (gboolean have_dual,
                                      guint64 option_flags, GCallback option_cb,
                                      guint64 auto_flags, GCallback auto_cb,
                                      GCallback exec_cb, GCallback write_cb, GCallback graph_cb, GCallback x_cb,
                                      gpointer cb_data,
                                      const gchar *control_id);
        // tmp use
        void add_to_remote_list (Gtk_EntryControl *ecx, const gchar *rid) {
                remote_list_ec = ecx->AddEntry2RemoteList(rid, remote_list_ec);
        };

        void add_to_scan_freeze_widget_list (GtkWidget *w){
                scan_freeze_widget_list = g_slist_prepend (scan_freeze_widget_list, w);
        };
        static void update_widget (GtkWidget *w, gpointer data){
                gtk_widget_set_sensitive (w, GPOINTER_TO_INT(data));
        };
        void scan_start_gui_actions (){
                g_message ("DSP GUI UDPATE SCAN START");
                g_slist_foreach (scan_freeze_widget_list, (GFunc) GUI_Builder::update_widget, GINT_TO_POINTER(FALSE));
        };
        void scan_end_gui_actions (){
                g_message ("DSP GUI UDPATE SCAN END");
                g_slist_foreach (scan_freeze_widget_list, (GFunc) GUI_Builder::update_widget, GINT_TO_POINTER(TRUE));
        };

        GSList *get_config_checkbutton_list_head () { return config_checkbutton_list; };

        GSList *config_checkbutton_list;
        GtkWidget *scrolled_contents;
	GSList *scan_freeze_widget_list;
        
        GtkWidget *wid;
        GtkWidget *tg;
        GtkWidget *page;
};

#define N_GVP_VECTORS 25 //  vectors max total, need a few extra for controls and finish.

// GUI for hardware template specific controls
class RPSPMC_Control : public AppBase{
public:
        RPSPMC_Control(Gxsm4app *app):AppBase(app){
                vpg_window = NULL;
                vpg_app_window = NULL;
                vpg_grid = NULL;

                // need to create according xml recource files for this to make work....
                hwi_settings = g_settings_new (GXSM_RES_BASE_PATH_DOT".hwi.rpspmc-control");
                Unity    = new UnitObj(" "," ");
                Volt     = new UnitObj("V","V");
                Velocity  = new UnitObj("px/s","px/s");
                dB       = new UnitObj("dB","dB");

                Angstroem= new UnitObj(UTF8_ANGSTROEM,"A");
                Frq      = new UnitObj("Hz","Hz");
                Time     = new UnitObj("s","s");
                TimeUms  = new LinUnit("ms","ms",1e-3);
                msTime   = new UnitObj("ms","ms");
                minTime  = new UnitObj("min","min");
                Deg      = new UnitObj(UTF8_DEGREE,"Deg");
                // Current  = new UnitAutoMag("A","A"); ((UnitAutoMag*) Current)->set_mag_get_base (1e-9, 1e-9); // nA default and internal "base"
                Current  = new UnitObj("nA","nA");
                Current_pA  = new UnitObj("pA","pA");
                Speed    = new UnitObj(UTF8_ANGSTROEM"/s","A/s");
                PhiSpeed = new UnitObj(UTF8_DEGREE"/s","Deg/s");
                Vslope   = new UnitObj("V/s","V/s");
                Hex      = new UnitObj("h","h");
               
                bias = 0;
                zpos_ref = 0;

                for (int i=0; i<4; ++i){
                        scan_source[i] = i;
                        probe_source[i] = i;
                        
                        mix_fbsource[i] = 0;
                        mix_unit2volt_factor[i] = 1;
                        mix_set_point[i] = 0;
                        mix_gain[i] = 1;
                        mix_level[i] = 0;
                        mix_transform_mode[i] = 0;
                }

                z_servo[0] = 0;
                z_servo[1] = 0;
                z_servo[2] = 0;

                fast_return = 0;
                x2nd_Zoff = 0;

                ldc_flag = 0;
                mirror_dsp_scan_dx32 = 1;
                mirror_dsp_scan_dy32 = 1;
        
                area_slope_x = 0;
                area_slope_y = 0;
                area_slope_compensation_flag = 0;
                center_return_flag = 0;

                move_speed_x = 500;
                scan_speed_x_requested = 500;
                scan_speed_x = 500;

                ue_bias = 0;
                ue_set_point[0] = 0;
                ue_set_point[1] = 0;
                ue_set_point[2] = 0;
                ue_set_point[3] = 0;
                ue_z_servo[0] = 0;
                ue_z_servo[1] = 0;
                ue_z_servo[2] = 0;
                ue_scan_speed_x_r = 0;
                ue_scan_speed_x = 0;
                ue_slope_x = 0;
                ue_slope_y = 0;
                ue_slope_flg = 0;

                dxdt = 0;
                dydt = 0;
                dzdt = 0;

                
                // LockIn
                AC_amp[0] = 0;
                AC_amp[1] = 0;
                AC_amp[2] = 0;
                AC_amp[3] = 0;
                AC_frq = 330;

	// Graphs Folder -- user settings origin
                Source = XSource = PSource = 0;
                XJoin = GrMatWin = 0;
                PlotAvg = PlotSec = 0;

	// Graphs used life -- dep. on GLOCK if copy of user settings or memorized last setting
                vis_Source = vis_XSource = vis_XJoin = vis_PSource = 0;
                vis_PlotAvg = vis_PlotSec = 0;


	// STS (I-V)
                IV_start = -1, IV_end = 1, IV_slope = 0.1, IV_slope_ramp = 0.5, IV_final_delay=0.1, IV_recover_delay=0.1;
                IV_points = 2000;
                IV_repetitions = 1;

                for (int i=0; i<N_GVP_VECTORS; ++i){
                        GVP_du[i] = 0;
                        GVP_dx[i] = 0;
                        GVP_dy[i] = 0;
                        GVP_dz[i] = 0;
                        GVP_da[i] = 0;
                        GVP_db[i] = 0;
                        GVP_ts[i] = 0;
                        GVP_points[i] = 0;
                        GVP_opt[i] = 0;
                        GVP_data[i] = 0;
                        GVP_vnrep[i] = 0;
                        GVP_vpcjr[i] = 0;
                }
                
                // init all vars with last used values is done via dconf / schemata
                // -- BUT not at very first generation via auto write schemata, will get random memory eventually to edit manually later....
                
                sim_speed[0]=sim_speed[1]=2000.0; // per tab
                sim_bias[0]=sim_bias[1]=1.0;
                options = 0x03;

                get_tab_settings ("IV", IV_option_flags, IV_auto_flags, IV_glock_data);
                get_tab_settings ("VP", GVP_option_flags, GVP_auto_flags, GVP_glock_data);
                GVP_restore_vp ("VP_set_last"); // last in view

                vp_exec_mode_name = NULL;
                
                init_vp_signal_info_lookup_cache();

                for (int i=0; i<10; ++i)
                        VPprogram[i] = NULL;

                // init pc matrix to NULL
                for (int i=0; i < 2*MAX_NUM_CHANNELS; ++i) 
                        for (int j=0; j < 2*MAX_NUM_CHANNELS; ++j) 
                                probe_pc_matrix[i][j] = NULL;

                IV_status = NULL;
                GVP_status = NULL;

                write_vector_mode=PV_MODE_NONE;

                probedata_list = NULL;
                probehdr_list = NULL;
                num_probe_events = 0;
                last_probe_data_index = 0;
                nun_valid_data_sections = 0;
                pv_lock = FALSE;

                probe_trigger_single_shot = 0;
                current_probe_data_index = 0;
                current_probe_section = 0;
                
                for (int i=0; i<NUM_PROBEDATA_ARRAYS; ++i)
                        garray_probedata [i] = NULL;
                
                for (int i=0; i<NUM_PROBEDATA_ARRAYS; ++i)
                        garray_probe_hdrlist [i] = NULL;
                
                DSP_vpdata_ij[0]=2; // DSP level VP data acccess indexing, global
                DSP_vpdata_ij[1]=0;

                memset (&program_vector, 0, sizeof(program_vector));
                
                create_folder ();
        };
	virtual ~RPSPMC_Control() {
                delete Unity;
                delete Volt;
                delete Velocity;
                delete dB;
                
                delete Angstroem;
                delete Frq;
                delete Time;
                delete TimeUms;
                delete msTime;
                delete minTime;
                delete Deg;
                delete Current;
                delete Current_pA;
                delete Speed;
                delete PhiSpeed;
                delete Vslope;
                delete Hex;
        };

        virtual void AppWindowInit(const gchar *title);
	void create_folder();

	void get_tab_settings (const gchar *tab_key, guint64 &option_flags, guint64 &AC_auto_flags, guint64 glock_data[6]);
        void set_tab_settings (const gchar *tab_key, guint64 option_flags, guint64 AC_auto_flags, guint64 glock_data[6]);

	void save_values (NcFile *ncf);
        void load_values (NcFile *ncf);

        void store_values ();

        static int config_options_callback (GtkWidget *widget, RPSPMC_Control *dspc);
        
	//static void ChangedWaveOut(Param_Control* pcs, gpointer data);
	//static int config_waveform (GtkWidget *widget, RPSPMC_Control *spmsc);
	static void configure_callback (GSimpleAction *simple, GVariant *parameter, gpointer user_data);
        static int choice_Ampl_callback(GtkWidget *widget, RPSPMC_Control *spmsc);
        static int choice_scansource_callback (GtkWidget *widget, RPSPMC_Control *dspc);

	static int DSP_cret_callback (GtkWidget *widget, RPSPMC_Control *dspc);
	static int DSP_slope_callback (GtkWidget *widget, RPSPMC_Control *dspc);

        static int ldc_callback(GtkWidget *widget, RPSPMC_Control *dspc);

        static void lockin_adjust_callback(Param_Control* pcs, gpointer data);
        static int lockin_runfree_callback(GtkWidget *widget, RPSPMC_Control *dspc);

        static void show_tab_to_configure (GtkWidget* w, gpointer data){
                gtk_widget_show (GTK_WIDGET (g_object_get_data (G_OBJECT (w), "TabGrid")));
        };
        static void show_tab_as_configured (GtkWidget* w, gpointer data){
                if (gtk_check_button_get_active (GTK_CHECK_BUTTON (w)))
                        gtk_widget_show (GTK_WIDGET (g_object_get_data (G_OBJECT (w), "TabGrid")));
                else
                        gtk_widget_hide (GTK_WIDGET (g_object_get_data (G_OBJECT (w), "TabGrid")));
        };


        static void BiasChanged(Param_Control* pcs, RPSPMC_Control *self);
        static void ZPosSetChanged(Param_Control* pcs, RPSPMC_Control *self);
        static void ZServoParamChanged(Param_Control* pcs, RPSPMC_Control *self);
        static void ZServoControl(GtkWidget *widget, RPSPMC_Control *self);
        static void ZServoControlInv(GtkWidget *widget, RPSPMC_Control *self);

        static void ChangedNotify(Param_Control* pcs, gpointer data);
        static void ChangedNotifyVP(Param_Control* pcs, gpointer data);
        static int ChangedAction(GtkWidget *widget, RPSPMC_Control *dspc);
	void update_zpos_readings ();
	static guint refresh_zpos_readings(RPSPMC_Control *dspc);
        static int zpos_monitor_callback(GtkWidget *widget, RPSPMC_Control *dspc);
        static int choice_mixmode_callback (GtkWidget *widget, RPSPMC_Control *dspc);
        static int choice_prbsource_callback(GtkWidget *widget, RPSPMC_Control *dspc);

        static int auto_probe_callback(GtkWidget *widget, RPSPMC_Control *dspc);
	static int Probing_graph_callback(GtkWidget *widget, RPSPMC_Control *dspc, int finish_flag=0);
        static int Probing_event_setup_scan (int ch, const gchar *titleprefix, const gchar *name, const gchar *unit, const gchar *label, double d2u, int nvalues);
	static int Probing_eventcheck_callback(GtkWidget *widget, RPSPMC_Control *dspc);
	static int Probing_exec_ABORT_callback(GtkWidget *widget, RPSPMC_Control *dspc);
	static int Probing_save_callback(GtkWidget *widget, RPSPMC_Control *dspc);
	static int Probing_abort_callback(GtkWidget *widget, RPSPMC_Control *dspc);

	static int Probing_exec_IV_callback(GtkWidget *widget, RPSPMC_Control *dspc);
	static int Probing_write_IV_callback(GtkWidget *widget, RPSPMC_Control *dspc);
	static int callback_change_IV_option_flags (GtkWidget *widget, RPSPMC_Control *dspc);
	static int callback_change_IV_auto_flags (GtkWidget *widget, RPSPMC_Control *dspc);

	static int Probing_exec_GVP_callback(GtkWidget *widget, RPSPMC_Control *dspc);
	static int Probing_write_GVP_callback(GtkWidget *widget, RPSPMC_Control *dspc);
        
        static int callback_change_GVP_vpc_option_flags (GtkWidget *widget, RPSPMC_Control *dspc);
	static int callback_update_GVP_vpc_option_checkbox (GtkWidget *widget, RPSPMC_Control *dspc);
	static int callback_change_GVP_option_flags (GtkWidget *widget, RPSPMC_Control *dspc);
	static int callback_change_GVP_auto_flags (GtkWidget *widget, RPSPMC_Control *dspc);
	static int callback_edit_GVP (GtkWidget *widget, RPSPMC_Control *dspc);
	static int callback_GVP_store_vp (GtkWidget *widget, RPSPMC_Control *dspc);
	static int callback_GVP_restore_vp (GtkWidget *widget, RPSPMC_Control *dspc);

        static int change_source_callback (GtkWidget *widget, RPSPMC_Control *dspc);
	static int callback_XJoin (GtkWidget *widget, RPSPMC_Control *dspc);
	static int callback_GrMatWindow (GtkWidget *widget, RPSPMC_Control *dspc);

	static gboolean idle_callback (gpointer data){
		RPSPMC_Control *dspc = (RPSPMC_Control*) data;

                // make probe vector data is not locked for vector manipulations, wait until available (fast)
                while (dspc->pv_lock)
                        usleep (1000);

                dspc->gr_lock = TRUE;
		dspc->Probing_graph_callback (NULL, dspc, dspc->idle_callback_data_ff);
                dspc->gr_lock = FALSE;
                
		dspc->idle_id = 0; // done.

		return FALSE;
	};
		
	void Probing_graph_update_thread_safe (int finish_flag=0) {
                static int timeout_count=0;
                static int last_index=0;
                static int count=0;
                //g_message ("Probing_graph_update_thread_safe: current index=%d, last=%d", current_probe_data_index, last_index);

                if (current_probe_data_index <= last_index && !finish_flag){
                        // g_message ("Probing_graph_update_thread_safe: exit no new data");
                        return;
                }
                last_index = current_probe_data_index;
                ++count;
                // call: Probing_graph_callback (NULL, this, finish_flag);
                // check for --  idle_id ??
                while (idle_id && finish_flag){
                        //g_message ("Probing_graph_update_thread_safe: Finish_flag set -- waiting for last update to complete. current index=%d, last=%d, #toc=%d",
                        //            current_probe_data_index, last_index, timeout_count);
                        usleep(250000);
                        ++timeout_count;
                        
                        if (last_index == current_probe_data_index && timeout_count > 10){
                                g_warning ("Probing_graph_update_thread_safe: Trying auto recovery from stalled update (timeout reached). current index=%d, last=%d",
                                         current_probe_data_index, last_index);
                                idle_id = 0;
                                timeout_count = 0;
                        }
                        
                }
                if (idle_id == 0){
                        // g_message ("Probing_graph_update_thread_safe: plot update. delayed by %d attempts. Current data index=%d",
                        //           count, current_probe_data_index);
                        count=0;
                        idle_callback_data_ff = finish_flag;
                        idle_id = g_idle_add (RPSPMC_Control::idle_callback, this);
                        if (finish_flag){
                                //g_message ("Probing_graph_update_thread_safe: plot update. Finished. Current data index=%d",
                                //           current_probe_data_index);
                                last_index = 0;
                        }
                } else {
                        // g_warning ("Probing_graph_update_thread_safe: is busy, skipping. [%d]", count);
                }
	};



	//int check_vp_in_progress (const gchar *extra_info=NULL);



        
	int probedata_length () { return current_probe_data_index; };
	void push_probedata_arrays ();
	GArray** pop_probedata_arrays ();
	GArray** pop_probehdr_arrays ();
	void init_probedata_arrays ();
	static void free_probedata_array_set (GArray** garr, RPSPMC_Control *dc);
	static void free_probehdr_array_set (GArray** garr, RPSPMC_Control *dc);
	void free_probedata_arrays ();

	int check_vp_in_progress (const gchar *extra_info=NULL) { return 0; }; // DUMMY -- check controller for !!

	void add_probedata(double data[NUM_PV_DATA_SIGNALS], double pv[NUM_PV_HEADER_SIGNALS], gboolean set_pv=false, gboolean add_pv=true);
        // "set" and append PROBEDATA_ARRAY_INDEX, BLOCK, HEADER:[X,Y,Z,U,A,B]
	void set_probevector(double pv[NUM_PV_HEADER_SIGNALS]);
        // append PROBEDATA_ARRAY_INDEX, SEC, BLOCK and  "add" to (emulate GVP signal generation) TIME, X,Y,Z,U,A,B, and append
	void add_probevector();

        int next_section(int pc){
                 if (pc < 0)
                         return 0; // start
                 else {
                         if (program_vector_list[pc].iloop > 0){
                                 if (--program_vector_list[pc].iloop > 0)
                                         pc += program_vector_list[pc].ptr_next; // jump
                                 else {
                                         program_vector_list[pc].iloop = program_vector_list[pc].repetitions; // reload
                                         pc++; // and proceed to next
                                 }
                         } else {
                                 pc++; // next
                         }
                         if ( pc >= MAX_PROGRAM_VECTORS ||  pc < 0){ // pc exception check (out of valid vpc range)
                                 pc=0; // reset
                                 g_warning ("Inernal GVP_vp_header_current.section (vpc) program counter position / jump out of range.");
                         }
                 }
                 return pc;
        };
        
	void add_probe_hdr(double pv[NUM_PV_HEADER_SIGNALS]);
	void dump_probe_hdr();

	void probedata_visualize (GArray *probedata_x, GArray *probedata_y,  GArray *probedata_sec, 
				  ProfileControl* &pc, ProfileControl* &pc_av, int plot_msk,
				  const gchar *xlab, const gchar *xua, double xmult,
				  const gchar *ylab, const gchar *yua, double ymult,
				  int current_i, int si, int nas, gboolean join_same_x=FALSE,
                                  gint xmap=0, gint src=0, gint num_active_xmaps=1, gint num_active_sources=1);

        int   msklookup[NUM_PROBEDATA_ARRAYS+1];
        int   expdi_lookup[NUM_PROBEDATA_ARRAYS+1];
        char* lablookup[NUM_PROBEDATA_ARRAYS+1];
        char* unitlookup[NUM_PROBEDATA_ARRAYS+1];
        double scalelookup[NUM_PROBEDATA_ARRAYS+1];
        
        double pv_tmp[NUM_PV_HEADER_SIGNALS];

        void init_vp_signal_info_lookup_cache();
   	const char* vp_label_lookup(int i);
	const char* vp_unit_lookup(int i);
	double      vp_scale_lookup(int i);

        void update_GUI(); // update GUI
           
	static gboolean idle_callback_update_gui (gpointer data){
		RPSPMC_Control *dspc = (RPSPMC_Control*) data;
                gapp->spm_update_all();
                gapp->SetStatus(N_("Saved VP data: "), dspc->idle_callback_data_fn);
                gchar *bbt = g_strdup_printf ("Save now - last: %s", dspc->idle_callback_data_fn);
                gtk_button_set_label (GTK_BUTTON (dspc->save_button), bbt);
                g_free (bbt);
                g_free (dspc->idle_callback_data_fn); // clean up tmp data now
                dspc->idle_callback_data_fn = NULL;
		dspc->idle_id_update_gui = 0; // done.
		return FALSE;
	};
		
	void update_gui_thread_safe (const gchar *fntmp) {
                // execute GUI updated thread safe
                if (idle_id_update_gui == 0){
                        idle_callback_data_fn = g_strdup (fntmp);
                        idle_id_update_gui = g_idle_add (RPSPMC_Control::idle_callback_update_gui, this);
                } else {
                        g_warning ("update_gui_thread_safe: is busy, skipping [%s].", fntmp);
                }
	};

        void update_controller ();
        
        GUI_Builder *bp;

        // SPM parameters (GUI)

  	double bias;           //!< STM Bias (usually applied to the sample)

	Gtk_EntryControl *ZPos_ec;
   	double zpos_ref;
	gint   zpos_refresh_timer_id;
	  
	// -- FEEDBACK MIXER --
	int    mix_fbsource[4];   // only for documentation
	double mix_unit2volt_factor[4]; // on default: [0] Current Setpoint (STM; log mode) [1..3]: off
	double mix_set_point[4]; // on default: [0] Current Setpoint (STM; log mode) [1..3]: off
	double mix_gain[4];      // Mixing Gains: 0=OFF, 1=100%, note: can be > 1 or even negative for scaling purposes
	double mix_level[4];     // fuzzy mixing control via level, applied only if fuzzy flag set
	// -- may not yet be applied to all signal --> check with DSP code
	int    mix_transform_mode[4]; //!< transformation mode on/off log/lin iir/full fuzzy/normal

        // Feedback (Z-Servo)
	double z_servo[3];    // Z-Servo (Feedback) [0] (not used here), [1] Const Proportional, [2] Const Integral [user visible values]

	int    scan_source[4];    // scan source mapping signal index for imaging
	int    probe_source[4];   // probe source mapping signal index for 32bit data channels [0..3]

	int    vp_input_id_cache[4];  // cache VP input config;
	int    DSP_vpdata_ij[2];
	GtkWidget *VPSig_menu, *VPSig_mi[8], *VPScanSrcVPitem[4];
        
	double fast_return;       //!< on-the-fly fast return option (scan retrace speed override factor, 1=normal)
	double x2nd_Zoff;         //!< Z lift off for 2nd scan line (MFM etc...)

	int    ldc_flag;          //! LDC status at last update
	GtkWidget *LDC_status;    //!< linear drift correction flag (on/off)

        // -- DSP Scan DNX,DNY,... mirrors
        gint32 mirror_dsp_scan_dx32, mirror_dsp_scan_dy32;
        gint32 dsp_scan_dnx, dsp_scan_dny;
        gint32 dsp_scan_fs_dx, dsp_scan_fs_dy;
        gint32 dsp_scan_fast_return;
        double scanpixelrate;
        
        // Scan Slope Compensation Parameters
        double area_slope_x;      //!< slope compensation in X, in scan coordinate system (possibly rotated) -- applied before, but by feedback
	double area_slope_y;      //!< slope compensation in Y, in scan coordinate system (possibly rotated) -- applied before, but by feedback
	Gtk_EntryControl *slope_x_ec;
	Gtk_EntryControl *slope_y_ec;
	int    area_slope_compensation_flag; //!< enable/disable slope compensation
	int    center_return_flag; //!< enable/disable return to center after scan

        // Move and Scan Speed
        double move_speed_x;      //!< in DAC (AIC) units per second, GXSM core computes from A/s using X-gain and Ang/DAC...
	double scan_speed_x_requested;      //!< in DAC (AIC) units per second - requested
	double scan_speed_x;      //!< in DAC (AIC) units per second - best match
	Gtk_EntryControl *scan_speed_ec;

        double dxdt, dydt, dzdt;

        
	// UserEvent sensitive:
	double ue_bias;
	double ue_set_point[4];
	double ue_z_servo[3];
	double ue_scan_speed_x_r;
	double ue_scan_speed_x;
	double ue_slope_x;
	double ue_slope_y;
	double ue_slope_flg;

	// LockIn
	double    AC_amp[4], AC_frq, AC_phaseA, AC_phaseB;
	Gtk_EntryControl *AC_frq_ec;
	guint64   AC_option_flags;
	guint64   AC_auto_flags;
	GtkWidget *LockIn_mode;
	GtkWidget *AC_status;
	guint64   AC_glock_data[6];

	// Probing
	int probe_trigger_raster_points_user;
	int probe_trigger_raster_points;
	int probe_trigger_raster_points_b;
	int probe_trigger_single_shot;

	// Graphs Folder -- user settings origin
	int Source, XSource, PSource;
        gboolean XJoin, GrMatWin;
	int PlotAvg, PlotSec;

	// Graphs used life -- dep. on GLOCK if copy of user settings or memorized last setting
	int vis_Source, vis_XSource, vis_XJoin, vis_PSource;
	int vis_PlotAvg, vis_PlotSec;

	int probe_ready;
	gchar *probe_fname;
	int probe_findex;

	// vector generation helpers
	void make_auto_n_vector_elments (double fnum);
	double make_Vdz_vector (double Ui, double Uf, double dZ, int n, double slope, int source, int options, double &duration, make_vector_flags flags);
	double make_Vdx0_vector (double Ui, double Uf, double dZ, int n, double slope, int source, int options, double &duration, make_vector_flags flags);
	double make_dx0_vector (double X0i, double X0f, int n, double slope, int source, int options, double &duration, make_vector_flags flags);
	double make_ZXYramp_vector (double dZ, double dX, double dY, int n, double slope, int source, int options, double &duration, make_vector_flags flags);
	double make_UZXYramp_vector (double dU, double dZ, double dX, double dY, double da, double db, int n, int nrep, int ptr_next, double ts, int source, int options);
	double make_phase_vector (double dPhi, int n, double slope, int source, int options, double &duration, make_vector_flags flags);
	double make_delay_vector (double delay, int source, int options, double &duration, make_vector_flags flags, int points=0);
	void append_null_vector (int options, int index);

	PROBE_VECTOR_GENERIC program_vector;

	// STS (I-V)
	double IV_start, IV_end, IV_slope, IV_slope_ramp, IV_final_delay, IV_recover_delay;
	int    IV_points;
	int    IV_repetitions;
	guint64 IV_option_flags;
	guint64 IV_auto_flags;
	GtkWidget *IV_status;
	guint64 IV_glock_data[6];


	// GVP (General Vector Probe)
#define N_GVP_VECTORS MAX_PROGRAM_VECTORS
	double GVP_du[N_GVP_VECTORS], GVP_dx[N_GVP_VECTORS], GVP_dy[N_GVP_VECTORS], GVP_dz[N_GVP_VECTORS];
        double GVP_da[N_GVP_VECTORS],  GVP_db[N_GVP_VECTORS];
        double GVP_ts[N_GVP_VECTORS];
	gint32 GVP_points[N_GVP_VECTORS];
	gint32 GVP_opt[N_GVP_VECTORS];   // options
	gint32 GVP_data[N_GVP_VECTORS];  // GPIO data
	gint32 GVP_vnrep[N_GVP_VECTORS]; // Vector N repetitions
	gint32 GVP_vpcjr[N_GVP_VECTORS]; // VPC jump relative length
	guint64    GVP_option_flags;
	guint64    GVP_auto_flags;
	GtkWidget *VPprogram[10];
	GtkWidget *GVP_status;
	guint64    GVP_glock_data[6];
	void GVP_store_vp (const gchar *key);
	void GVP_restore_vp (const gchar *key);

        void write_spm_scan_vector_program (double rx, double ry, int nx, int ny, double slew[2], int subscan[4], long int srcs[4]);

        
	// -- Profile Displays
	int last_probe_data_index;

	// dynamic temporary probe data storage
	GSList *probedata_list;
	GSList *probehdr_list;
	int num_probe_events;
	// -- Array of full expanded probe data set
	GArray *garray_probe_hdrlist[NUM_PROBEDATA_ARRAYS];
	GArray *garray_probedata[NUM_PROBEDATA_ARRAYS];
	int current_probe_data_index;
        int current_probe_block_index;
        int current_probe_section;
	int nun_valid_data_sections;
	int nun_valid_hdr, last_nun_hdr_dumped;

	// The factor of 2 is to have it big enough to hold also the averaged data
	ProfileControl *probe_pc_matrix[2*MAX_NUM_CHANNELS][2*MAX_NUM_CHANNELS];

	gchar *vp_exec_mode_name;

	guint64    current_auto_flags;
	guint64    raster_auto_flags;
	GtkWidget *save_button;

	gboolean        pv_lock;
	gboolean        gr_lock;


        
        // Simultaor only (GUI)
        double sim_bias[2];  // per tab -- simulation
        double sim_speed[2]; // per tab -- simulation
        gint options;
        

        
protected:
	void read_spm_vector_program ();
	void write_spm_vector_program (int start, pv_mode pvm);

        
	void read_program_vector (int index);
	void write_program_vector (int index);

	void write_dsp_abort_probe ();

        int idle_callback_data_ff;
        guint idle_id;

        gchar *idle_callback_data_fn;
        guint idle_id_update_gui;

	gboolean GUI_ready;

private:
	PROBE_VECTOR_GENERIC     program_vector_list[MAX_PROGRAM_VECTORS]; // copy for GXSM internal use only

	GSettings *hwi_settings;

        pv_mode write_vector_mode;
        
        // ==== VP graphs organizer
        Gxsm4appWindow *vpg_app_window;
	GtkWindow* vpg_window;
        GtkWidget* vpg_grid;

	UnitObj *Unity, *Volt, *Velocity, *dB;
        UnitObj *Angstroem, *Frq, *Time, *TimeUms, *msTime, *minTime, *Deg, *Current, *Current_pA, *Speed, *PhiSpeed, *Vslope, *Hex;
};



/*
 * RPSPMC hardware interface class -- derived from GXSM XSM_hardware abstraction class
 * =======================================================================
 */
class rpspmc_hwi_dev : public XSM_Hardware, public RP_stream{

public: 
	friend class RPSPMC_Control;

	rpspmc_hwi_dev();
	virtual ~rpspmc_hwi_dev();

        static void spmc_stream_connect_cb (GtkWidget *widget, rpspmc_hwi_dev *self);
        virtual const gchar *get_rp_address ();
        virtual void status_append (const gchar *msg, bool schedule_from_thread=false);
        virtual void on_connect_actions();
        virtual int on_new_data (gconstpointer contents, gsize len, int position, int new_count=1, bool last=false);
        
	/* Parameter  */
	virtual long GetMaxLines(){ return 32000; };

	virtual const gchar* get_info() { return "SPM Template V0.0"; };

	/* Hardware realtime monitoring -- all optional */
	/* default properties are
	 * "X" -> current realtime tip position in X, inclusive rotation and offset
	 * "Y" -> current realtime tip position in Y, inclusive rotation and offset
	 * "Z" -> current realtime tip position in Z
	 * "xy" -> X and Y
	 * "zxy" -> Z, X, Y  in Volts!
         * "O" -> Offset -> z0, x0, y0 (Volts)
         * "f0I" -> 
	 * "U" -> current bias
         * "W" -> WatchDog
	 */
	virtual gint RTQuery (const gchar *property, double &val1, double &val2, double &val3);

	virtual gint RTQuery () { return 0; }; //spm_emu->data_y_index + subscan_data_y_index_offset; }; // actual progress on scan -- y-index mirror from FIFO read

	/* high level calls for instrtument condition checks */
	virtual gint RTQuery_clear_to_start_scan (){ return 1; };
	virtual gint RTQuery_clear_to_start_probe (){ return 1; };
	virtual gint RTQuery_clear_to_move_tip (){ return 1; };

	int is_scanning() { return ScanningFlg; };

	virtual double GetUserParam (gint n, gchar *id=NULL) { return 0.; };
	virtual gint   SetUserParam (gint n, gchar *id=NULL, double value=0.) { return 0; };
	
	virtual double GetScanrate () { return 1.; }; // query current set scan rate in s/pixel

	virtual int RotateStepwise(int exec=1); // rotation not implemented in simulation, if required set/update scan angle here

	virtual gboolean SetOffset(double x, double y); // set offset to coordinated (non rotated)
	virtual gboolean MovetoXY (double x, double y); // set tip position in scan coordinate system (potentially rotated)

	virtual void StartScan2D() { PauseFlg=0; ScanningFlg=1; KillFlg=FALSE; };
        // EndScan2D() is been called until it returns TRUE from scan control idle task until it returns FALSE (indicating it's completed)
	virtual gboolean EndScan2D() { ScanningFlg=0; GVP_abort_vector_program (); return FALSE; };
	virtual void PauseScan2D()   { PauseFlg=1; };
	virtual void ResumeScan2D()  { PauseFlg=0; };
	virtual void KillScan2D()    { PauseFlg=0; KillFlg=TRUE; };

        // ScanLineM():
        // Scan setup: (yindex=-2),
        // Scan init: (first call with yindex >= 0)
        // while scanning following calls are progress checks (return FALSE when yindex line data transfer is completed to go to next line for checking, else return TRUE to continue with this index!
	virtual gboolean ScanLineM(int yindex, int xdir, int muxmode,
				   Mem2d *Mob[MAX_SRCS_CHANNELS],
				   int ixy_sub[4]);

        int start_data_read (int y_start, 
                             int num_srcs0, int num_srcs1, int num_srcs2, int num_srcs3, 
                             Mem2d **Mob0, Mem2d **Mob1, Mem2d **Mob2, Mem2d **Mob3);

	virtual int ReadProbeData (int dspdev=0, int control=0);

        int GVP_expect_header(double *pv, int &index_all);
        int GVP_expect_point(double *pv, int &index_all);


        
	int probe_fifo_thread_active;
	int fifo_data_y_index;

	int fifo_data_num_srcs[4]; // 0: XP, 1: XM, 2: 2ND_XP, 3: 2ND_XM
	Mem2d **fifo_data_Mobp[4]; // 0: XP, 1: XM, 2: 2ND_XP, 3: 2ND_XM


        
        // dummy template signal management
	// SIGNAL MANAGEMENT

	virtual void read_dsp_signals () { read_signal_lookup (); read_actual_module_configuration (); };

	virtual int lookup_signal_by_ptr(gint64 sigptr);
	virtual int lookup_signal_by_name(const gchar *sig_name);
	virtual const gchar *lookup_signal_name_by_index(int i);
	virtual const gchar *lookup_signal_unit_by_index(int i);
	virtual double lookup_signal_scale_by_index(int i);
	virtual int change_signal_input(int signal_index, gint32 input_id, gint32 voffset=0);
	virtual int query_module_signal_input(gint32 input_id);
	int read_signal_lookup ();
	int read_actual_module_configuration ();

	DSP_SIG_UNIVERSAL *lookup_dsp_signal_managed(gint i){
                if (i<NUM_SIGNALS_UNIVERSAL)
                        return &dsp_signal_lookup_managed[i];
                else
                        return NULL;
	};

        int read_GVP_data_block_to_position_vector (int offset, gboolean expect_full_header=false){
                static int retry = 3;
                size_t ch_index;

#if 0
                if (expect_full_header || offset==0)
                        status_append_int32 (&GVP_stream_buffer[offset], 10*16, true, offset, true);
#endif
                
#if 0
                if (offset < 0 || offset > (EXPAND_MULTIPLES*DMA_SIZE-20)){
                        gchar *tmp = g_strdup_printf ("read_GVP_data_block_to_position_vector: Reading offset %08x out of range ERROR.",
                                                      offset);
                        status_append (tmp, true);
                        g_warning (tmp);
                        g_free (tmp);
                        return -999;
                }
#endif
                
                if (offset >= GVP_stream_buffer_position){ // Buffer is huge now all pages concat
#if 0
                        gchar *tmp = g_strdup_printf ("read_GVP_data_block_to_position_vector: Reading offset %08x is beyond stream write position %08x. Awaiting data.\n",
                                                      offset, GVP_stream_buffer_position);
                        status_append (tmp, true);
                        if (offset > 64)
                                status_append_int32 (&GVP_stream_buffer[offset-64], 10*16, true, offset-64, true);
                        else
                                status_append_int32 (&GVP_stream_buffer[0], 10*16, true, 0, true);
                        g_warning (tmp);
                        g_free (tmp);
#endif
                        return -99; // OK -- but have wait and reprocess when data package is completed
                }
                
                GVP_vp_header_current.srcs = GVP_stream_buffer[offset]&0xffff;

                GVP_vp_header_current.i = (int)(((guint32)GVP_stream_buffer[offset])>>16);
                if (GVP_vp_header_current.srcs == 0xffff){
                        GVP_vp_header_current.n    = GVP_vp_header_current.i + 1;
                        GVP_vp_header_current.endmark = 0;
                        retry=3;
                } else {
                        if (GVP_stream_buffer[offset] == 0xfefefefe){
                                GVP_vp_header_current.endmark = 1;
                                GVP_vp_header_current.n = 0;
                                GVP_vp_header_current.i = 0;
                                GVP_vp_header_current.srcs = 0xffff;
                        } else {
                                if (GVP_vp_header_current.n = GVP_vp_header_current.i+2)
                                        GVP_vp_header_current.srcs_mask_vector = GVP_vp_header_current.srcs;
                                else {
                                        if (GVP_vp_header_current.i != (GVP_vp_header_current.ilast+1)
                                            ||
                                            GVP_vp_header_current.srcs_mask_vector != GVP_vp_header_current.srcs){
                                                // stream ERROR detected
                                                gchar *tmp = g_strdup_printf ("read_GVP_data_block_to_position_vector: Stream ERROR at Reading offset %08x, write position %08x.\n"
                                                                              "SRCS/index mismatch detected. %04x vs %04x, i %d -> %d\n",
                                                                              offset, GVP_stream_buffer_position,
                                                                              GVP_vp_header_current.srcs_mask_vector, GVP_vp_header_current.srcs,
                                                                              GVP_vp_header_current.ilast, GVP_vp_header_current.i);
                                                status_append (tmp, true);
                                                if (offset > 64)
                                                        status_append_int32 (&GVP_stream_buffer[offset-64], 10*16, true, offset-64, true);
                                                else
                                                        status_append_int32 (&GVP_stream_buffer[0], 10*16, true, 0, true);
                                                g_warning (tmp);
                                                g_free (tmp);
                                                return (-98);
                                        }
                                }
                        }
                }
                GVP_vp_header_current.ilast = GVP_vp_header_current.i;
                        
                GVP_vp_header_current.index = GVP_vp_header_current.n - GVP_vp_header_current.i;
                if (GVP_vp_header_current.index < 0){
                        gchar *tmp = g_strdup_printf ("read_GVP_data_block_to_position_vector: Stream ERROR at Reading offset %08x, write position %08x.\n"
                                                      "SRCS/index mismatch detected. %04x vs %04x, i %d -> %d  => n=%d (<0 is illegal)\n",
                                                      offset, GVP_stream_buffer_position,
                                                      GVP_vp_header_current.srcs_mask_vector, GVP_vp_header_current.srcs,
                                                      GVP_vp_header_current.ilast, GVP_vp_header_current.i, GVP_vp_header_current.index);
                        status_append (tmp, true);
                        if (offset > 64)
                                status_append_int32 (&GVP_stream_buffer[offset-64], 10*16, true, offset-64, true);
                        else
                                status_append_int32 (&GVP_stream_buffer[0], 10*16, true, 0, true);
                        g_warning (tmp);
                        g_free (tmp);
                        GVP_vp_header_current.index = 0; // to prevent issues
                        if (--retry)
                                return -99;
                        else
                                return (-95);
                }
                
                GVP_vp_header_current.number_channels=0;
                for (int i=0; i<16; i++){
                        GVP_vp_header_current.ch_lut[i]=-1;
                        if (GVP_vp_header_current.srcs & (1<<i))
                                GVP_vp_header_current.ch_lut[GVP_vp_header_current.number_channels++] = i;
                }

                // expand data stream and sort into channels
                for (ch_index=0; ch_index < GVP_vp_header_current.number_channels && ch_index+offset < GVP_stream_buffer_position; ++ch_index){
                        int ich = GVP_vp_header_current.ch_lut[ch_index];
                        GVP_vp_header_current.chNs[ich] = GVP_stream_buffer[1+ch_index+offset];
                        if (ich < 14){
                                if (ich > 4)
                                        GVP_vp_header_current.dataexpanded[ich] = (double)GVP_vp_header_current.chNs[ich]; // raw // fix me -- apply conversion to units!
                                else
                                        GVP_vp_header_current.dataexpanded[ich] = rpspmc_to_volts (GVP_vp_header_current.chNs[ich]); // Volts
                                //g_message ("%g V", GVP_vp_header_current.dataexpanded[ich]);
                        }
                }

                if (expect_full_header && GVP_vp_header_current.srcs != 0xffff){
                        gchar *tmp = g_strdup_printf ("ERROR: read_GVP_data_block_to_position_vector: Reading offset %08x, write position %08x. Expecting full header but found srcs=%04x, i=%d rty=%d\n",
                                                      offset, GVP_stream_buffer_position,  GVP_vp_header_current.srcs, GVP_vp_header_current.i, retry);
                        status_append (tmp, true);
                        if (offset>64)
                                status_append_int32 (&GVP_stream_buffer[offset-64], 10*16, true, offset-64, true);
                        else
                                status_append_int32 (&GVP_stream_buffer[0], 10*16, true, 0, true);
                        g_warning (tmp);
                        g_free (tmp);
                        if (--retry)
                                return -99;
                        else
                                return (-97);
                }
                

#if 1
                if (GVP_vp_header_current.srcs == 0xffff){
                        GVP_vp_header_current.gvp_time = (((guint64)((guint32)GVP_vp_header_current.chNs[15]))<<32) | (guint64)((guint32)GVP_vp_header_current.chNs[14]);
                        GVP_vp_header_current.dataexpanded[14] = (double)GVP_vp_header_current.gvp_time/125e3;
                        if (GVP_vp_header_current.endmark)
                                g_message ("N[ENDMARK] GVP_vp_header_current.srcs=%04x   Bias=%8g V    t=%8g ms",
                                           GVP_vp_header_current.srcs,
                                           rpspmc_to_volts (GVP_vp_header_current.chNs[3]),
                                           GVP_vp_header_current.dataexpanded[14]
                                           );
                        else
                                g_message ("N[%4d / %4d] GVP_vp_header_current.srcs=%04x   Bias=%8g V    t=%8g ms",
                                           GVP_vp_header_current.index, GVP_vp_header_current.n, GVP_vp_header_current.srcs,
                                           rpspmc_to_volts (GVP_vp_header_current.chNs[3]),
                                           GVP_vp_header_current.dataexpanded[14]
                                           );
                }
#endif                        

                if (ch_index+offset < GVP_stream_buffer_position){
                        if (GVP_vp_header_current.srcs & 0xc000){
                                GVP_vp_header_current.gvp_time = (((guint64)((guint32)GVP_vp_header_current.chNs[15]))<<32) | (guint64)((guint32)GVP_vp_header_current.chNs[14]);
                                GVP_vp_header_current.dataexpanded[14] = (double)GVP_vp_header_current.gvp_time/125e3;
                        }
                        retry=3;
                        if (GVP_vp_header_current.srcs == 0xffff)
                                return -1; // true for full position header update
                        return ch_index;
                }  else {
                        // g_message ("[%08x] *** end of new data at ch=%d ** Must wait for next page/update send and retry.", offset, ch_index);
                        return -99;
                        // return ch_index; // number channels read until position
                }
        };
       
        int subscan_data_y_index_offset;

   	Mem2d **Mob_dir[4]; // reference to scan memory object (mem2d)
	long srcs_dir[4]; // souce channel coding
	int nsrcs_dir[4]; // number of source channes active
        gint ScanningFlg;
        gint PauseFlg;

protected:
	int thread_sim; // connection to SRanger used by thread
        DSP_SIG_UNIVERSAL dsp_signal_lookup_managed[NUM_SIGNALS_UNIVERSAL]; // signals, generic version

private:
	GThread *data_read_thread;
	GThread *probe_data_read_thread;
        gboolean KillFlg;


        gint32 GVP_stream_buffer[EXPAND_MULTIPLES*DMA_SIZE];
        int GVP_stream_buffer_offset;
        int GVP_stream_buffer_AB;
        int GVP_stream_buffer_position;
        int GVP_stream_status;
        
public:
        gint last_vector_index;
 
        //SPM_emulator *spm_emu; // DSP emulator for dummy data generation and minimal SPM behavior
        void GVP_execute_vector_program(); // non blocking
        void GVP_vp_init ();
        void GVP_start_data_read(); // non blocking
        
        PROBE_HEADER_POSITIONVECTOR GVP_vp_header_current;

	int GVP_read_program_vector(int i, PROBE_VECTOR_GENERIC *v){
		if (i >= MAX_PROGRAM_VECTORS || i < 0)
			return 0;
                // NOTE: READINGN BACK NOT YET USED NOR SUPPORTED BY RPSPMC !!!
		// memcpy (v, &vector_program[i], sizeof (PROBE_VECTOR_GENERIC));
		return -1;
	};
	int GVP_write_program_vector(int i, PROBE_VECTOR_GENERIC *v);
	void GVP_abort_vector_program ();

        void RPSPMC_set_bias (double bias) {};
        void RPSPMC_set_current_sp (double sp) {};

        gint RPSPMC_GVP_section_count;
        gint RPSPMC_GVP_n;
        
        gint RPSPMC_GVP_secn;
        gint RPSPMC_data_y_count;
        gint RPSPMC_data_z_value;
        gint RPSPMC_data_y_index;
        gint RPSPMC_data_x_index;

        gboolean abort_GVP_flag;
};

        



/* RP-PACPLL and RP-SPMC communication -> -dev */

// Scan Control Class based on AppBase
// -> AppBase provides a GtkWindow and some window handling basics used by Gxsm
class RPspmc_pacpll : public AppBase, public RP_JSON_talk{
public:

        RPspmc_pacpll(Gxsm4app *app); // create window and setup it contents, connect buttons, register cb's...
	virtual ~RPspmc_pacpll(); // unregister cb's
        int setup_scan (int ch, 
                        const gchar *titleprefix, 
                        const gchar *name,
                        const gchar *unit,
                        const gchar *label,
                        double d2u
                        );

        static void connect_cb (GtkWidget *widget, RPspmc_pacpll *self);

        static void enable_scope (GtkWidget *widget, RPspmc_pacpll *self);
        static void dbg_l1 (GtkWidget *widget, RPspmc_pacpll *self);
        static void dbg_l2 (GtkWidget *widget, RPspmc_pacpll *self);
        static void dbg_l4 (GtkWidget *widget, RPspmc_pacpll *self);

        
	static void scan_start_callback (gpointer user_data);
	static void scan_stop_callback (gpointer user_data);

	static void pac_tau_transport_changed (Param_Control* pcs, gpointer user_data);
	static void pac_tau_parameter_changed (Param_Control* pcs, gpointer user_data);
	static void pac_frequency_parameter_changed (Param_Control* pcs, gpointer user_data);
	static void pac_volume_parameter_changed (Param_Control* pcs, gpointer user_data);
        static void select_pac_lck_amplitude (GtkWidget *widget, RPspmc_pacpll *self);
        static void select_pac_lck_phase (GtkWidget *widget, RPspmc_pacpll *self);
        static void show_dF_control (GtkWidget *widget, RPspmc_pacpll *self);
        static void show_pulse_control (GtkWidget *widget, RPspmc_pacpll *self);
        static void qcontrol (GtkWidget *widget, RPspmc_pacpll *self);
	static void qc_parameter_changed (Param_Control* pcs, gpointer user_data);
	static void tune_parameter_changed (Param_Control* pcs, gpointer user_data);
	static void amp_ctrl_parameter_changed (Param_Control* pcs, gpointer user_data);
	static void phase_ctrl_parameter_changed (Param_Control* pcs, gpointer user_data);
	static void amplitude_gain_changed (Param_Control* pcs, gpointer user_data);
        static void amplitude_controller_invert (GtkWidget *widget, RPspmc_pacpll *self);
        static void amplitude_controller (GtkWidget *widget, RPspmc_pacpll *self);
	static void phase_gain_changed (Param_Control* pcs, gpointer user_data);
        static void phase_controller_invert (GtkWidget *widget, RPspmc_pacpll *self);
        static void phase_controller (GtkWidget *widget, RPspmc_pacpll *self);
        static void phase_unwrapping_always (GtkWidget *widget, RPspmc_pacpll *self);
        static void phase_unwrap_plot (GtkWidget *widget, RPspmc_pacpll *self);
        static void set_ss_auto_trigger (GtkWidget *widget, RPspmc_pacpll *self);

	static void dfreq_ctrl_parameter_changed (Param_Control* pcs, gpointer user_data);
	static void dfreq_gain_changed (Param_Control* pcs, gpointer user_data);
        static void dfreq_controller_invert (GtkWidget *widget, RPspmc_pacpll *self);
        static void dfreq_controller (GtkWidget *widget, RPspmc_pacpll *self);

	static void pulse_form_parameter_changed (Param_Control* pcs, gpointer user_data);
        static void pulse_form_enable (GtkWidget *widget, RPspmc_pacpll *self);

        static void choice_operation_callback (GtkWidget *widget, RPspmc_pacpll *self);
        static void choice_transport_ch12_callback (GtkWidget *widget, RPspmc_pacpll *self);
        static void choice_transport_ch3_callback (GtkWidget *widget, RPspmc_pacpll *self);
        static void choice_transport_ch4_callback (GtkWidget *widget, RPspmc_pacpll *self);
        static void choice_transport_ch5_callback (GtkWidget *widget, RPspmc_pacpll *self);
        static void scope_ac_ch1_callback (GtkWidget *widget, RPspmc_pacpll *self);
        static void scope_ac_ch2_callback (GtkWidget *widget, RPspmc_pacpll *self);
        static void scope_ac_ch3_callback (GtkWidget *widget, RPspmc_pacpll *self);
        static void scope_xy_on_callback (GtkWidget *widget, RPspmc_pacpll *self);
        static void scope_fft_on_callback (GtkWidget *widget, RPspmc_pacpll *self);
        static void scope_fft_time_zoom_callback (GtkWidget *widget, RPspmc_pacpll *self);

        static void scope_z_ch1_callback (GtkWidget *widget, RPspmc_pacpll *self);
        static void scope_z_ch2_callback (GtkWidget *widget, RPspmc_pacpll *self);

        static void scope_buffer_position_callback (GtkWidget *widget, RPspmc_pacpll *self);

        
        static void choice_update_ts_callback (GtkWidget *widget, RPspmc_pacpll *self);
        static void choice_trigger_mode_callback (GtkWidget *widget, RPspmc_pacpll *self);
        static void choice_auto_set_callback (GtkWidget *widget, RPspmc_pacpll *self);

	void send_all_parameters ();

        void save_values (NcFile *ncf);
        
	void update (); // window update (inputs, etc. -- here currently not really necessary)
        void update_monitoring_parameters ();

        static void graph_draw_function (GtkDrawingArea *area, cairo_t *cr,
                                         int             width,
                                         int             height,
                                         RPspmc_pacpll *self);
        void dynamic_graph_draw_function (GtkDrawingArea *area, cairo_t *cr,
                                          int             width,
                                          int             height);
        void stream_data ();


        virtual const gchar *get_rp_address (){
                return gtk_entry_buffer_get_text (GTK_ENTRY_BUFFER (gtk_entry_get_buffer (GTK_ENTRY (input_rpaddress))));
        };

        virtual void status_append (const gchar *msg);
        virtual void update_health (const gchar *msg=NULL);
        virtual void on_connect_actions(); // called after connection is made -- init, setup all, ...

        virtual int get_debug_level() { return debug_level; };
        
        virtual void on_new_data (){
                update_monitoring_parameters();
                gtk_widget_queue_draw (signal_graph_area);

                //self->stream_data ();
                update_health ();
        };
        
        double unwrap (int k, double phi);
        
        // 1000mV = 0dB, 1mV = -60dB 
        inline double dB_from_mV (double mv){
                return 20.*(log10 (fabs(mv)+1e-10)-3.);
        };
        inline double db_to_y (double db, double dB_hi, double y_hi, double dB_mags){
                return -y_hi*2*((db-dB_hi)/20./dB_mags+0.5);
        };
        inline double deg_to_y (double deg, double y_hi){
#if 0 // use auto extent of range
                if (fabs (deg) > 180.*deg_extend)
                        if (deg_extend < 4)
                                deg_extend++;
#endif
                return -y_hi*deg/180./deg_extend;
        };
        inline double freq_to_y (double df, double y_hi){
                return -y_hi*df/10.0;
        };
        inline double binary_to_y (double x, int bit, int ch, double y_hi, int bits=8, int num_ch=2){
                int    bitv = (double) (1 << bit);
                double on = 0.4 * (((int)(x) & bitv) ? 1 : 0);
                double pos = ch*y_hi/(num_ch-1);
                double bith = 0.8*y_hi/(num_ch-1)/bits;
                return -(-y_hi+y_hi*ch+bith*(bit+on));
        };
         
private:
        int ch_freq;
        int ch_ampl;
        int data_decimation;
        int data_shr;
        int data_shr_max;
        int streampos;
        int x,y;
        int streaming;
        int operation_mode;
        int channel_selections[7];
        int deg_extend;

        double bram_window_length; // scope window length in sec
        
        PACPLL_parameters parameters;
        PACPLL_signals signals;

        BuildParam *bp;
        
        gboolean run_scope;
        gboolean scope_xy_on;
        gboolean scope_fft_on;
        double scope_fft_time_zoom;
        gboolean scope_ac[5];
        double scope_z[2];
        double scope_dc_level[5];
        int transport;
        int bram_shift;
        double gain_scale[5];
        double time_scale[5];
        gboolean unwrap_phase_plot;
        double scope_width_points;
        double scope_height_points;
        GtkWidget *signal_graph_area;

        GtkWidget *update_ts_widget;
        GtkWidget *update_op_widget;
        GtkWidget *update_tr_widget;

        GtkWidget *signal_graph;
        GtkWidget *dF_control_frame;
        GtkWidget *pulse_control_frame;
        Gtk_EntryControl *input_ddsfreq;
        UnitObj *Unity, *Hz, *Deg, *VoltDeg, *Volt, *mVolt, *VoltHz, *dB, *Time, *mTime, *uTime;

	GSList*   SPMC_RemoteEntryList;

	GMutex mutex;
       
public:
        // moved controls to main tab
        GtkWidget *input_rpaddress;
        GtkWidget *text_status;
	GtkWidget *red_pitaya_health;
        gint debug_level; 
        double rp_verbose_level; 
        GSettings *inet_json_settings;
};

#endif
