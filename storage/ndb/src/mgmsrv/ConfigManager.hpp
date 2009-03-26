/* Copyright (C) 2008 Sun Microsystems, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef ConfigManager_H
#define ConfigManager_H

#include "MgmtThread.hpp"
#include "Config.hpp"
#include "ConfigSubscriber.hpp"
#include "MgmtSrvr.hpp"
#include "Defragger.hpp"

#include <ConfigRetriever.hpp>

#include <SignalSender.hpp>
#include <NodeBitmask.hpp>

#include <signaldata/ConfigChange.hpp>

class ConfigManager : public MgmtThread {
  const MgmtSrvr::MgmtOpts& m_opts;
  TransporterFacade* m_facade;

  NdbMutex *m_config_mutex;
  const Config * m_config;
  const Config * m_new_config;

  ConfigRetriever m_config_retriever;

  struct ConfigChangeState {
    enum States {
      IDLE,
      PREPARING,
      COMITTING,
      ABORT,
      ABORTING
    } m_current_state;

    ConfigChangeState() :
      m_current_state(IDLE) {}

    operator int() const { return m_current_state; }
  } m_config_change_state;

  void set_config_change_state(ConfigChangeState::States state);

  enum ConfigState {
    CS_UNINITIALIZED,

    CS_INITIAL,      // Initial config.ini, ie. no config.bin.X found

    CS_CONFIRMED,    // Started and all agreed
    CS_FORCED        // Forced start
  };

  ConfigState m_config_state;
  ConfigState m_previous_state;

  /* The original error that caused config change to be aborted */
  ConfigChangeRef::ErrorCode m_config_change_error;

  BlockReference m_client_ref;
  BaseString m_config_name;
  Config* m_prepared_config;

  NodeBitmask m_all_mgm;
  NodeBitmask m_started;
  NodeBitmask m_waiting_for;
  NodeBitmask m_checked;

  NodeId m_node_id;

  const char* m_configdir;

  Defragger m_defragger;

  /* Functions used from 'init' */
  static Config* load_init_config(const char*);
  static Config* load_init_mycnf(void);
  Config* load_config(void) const;
  Config* fetch_config(void);
  bool save_config(const Config* conf);
  bool save_config(void);
  bool saved_config_exists(BaseString& config_name) const;
  bool delete_saved_configs(void) const;
  bool failed_config_change_exists(void) const;
  Config* load_saved_config(const BaseString& config_name);
  NodeId find_nodeid_from_configdir(void);
  NodeId find_nodeid_from_config(void);
  bool init_nodeid(void);

  /* Set the new config and inform subscribers */
  void set_config(Config* config);
  Vector<ConfigSubscriber*> m_subscribers;

  /* Check config is ok */
  bool config_ok(const Config* conf);

  /* Functions for writing config.bin to disk */
  bool prepareConfigChange(const Config* config);
  void commitConfigChange();
  void abortConfigChange();

  /* Functions for starting config change from ConfigManager */
  void startInitConfigChange(SignalSender& ss);
  void startNewConfigChange(SignalSender& ss);

  /* CONFIG_CHANGE - controlling config change from other node */
  void execCONFIG_CHANGE_REQ(SignalSender& ss, SimpleSignal* sig);
  void execCONFIG_CHANGE_REF(SignalSender& ss, SimpleSignal* sig);
  void sendConfigChangeRef(SignalSender& ss, BlockReference,
                           ConfigChangeRef::ErrorCode) const;
  void sendConfigChangeConf(SignalSender& ss, BlockReference) const;

  /*
    CONFIG_CHANGE_IMPL - protocol for starting config change
    between nodes
  */
  void execCONFIG_CHANGE_IMPL_REQ(SignalSender& ss, SimpleSignal* sig);
  void execCONFIG_CHANGE_IMPL_REF(SignalSender& ss, SimpleSignal* sig);
  void execCONFIG_CHANGE_IMPL_CONF(SignalSender& ss, SimpleSignal* sig);
  void sendConfigChangeImplRef(SignalSender& ss, NodeId nodeId,
                               ConfigChangeRef::ErrorCode) const;
  bool sendConfigChangeImplReq(SignalSender& ss, const Config* conf);

  /*
    CONFIG_CHECK - protocol for exchanging and checking config state
    between nodes
  */
  void execCONFIG_CHECK_REQ(SignalSender& ss, SimpleSignal* sig);
  void execCONFIG_CHECK_CONF(SignalSender& ss, SimpleSignal* sig);
  void execCONFIG_CHECK_REF(SignalSender& ss, SimpleSignal* sig);
  void sendConfigCheckReq(SignalSender& ss, NodeBitmask to);
  void sendConfigCheckRef(SignalSender& ss, BlockReference to,
                          ConfigCheckRef::ErrorCode,
                          Uint32, Uint32, ConfigState, ConfigState) const;
  void sendConfigCheckConf(SignalSender& ss, BlockReference to) const;

  /*
    ConfigChecker - for connecting to other mgm nodes without
    transporter
  */
  class ConfigChecker : public MgmtThread {
    ConfigManager& m_manager;
    ConfigRetriever m_config_retriever;
    BaseString m_connect_string;
    NodeId m_nodeid;
  public:
    ConfigChecker(); // Not implemented
    ConfigChecker(const ConfigChecker&); // Not implemented
    ConfigChecker(ConfigManager& manager,
                  const char* connect_string,
                  const char* bind_address,
                  NodeId nodeid);
    bool init();
    virtual void run();
  };
  bool init_checkers(const Config* config);
  void start_checkers();
  void stop_checkers();
  Vector<ConfigChecker*> m_checkers;
  MutexVector<NodeId> m_exclude_nodes;
  void handle_exclude_nodes(void);

public:
  ConfigManager(const MgmtSrvr::MgmtOpts&,
                const char* configdir);
  virtual ~ConfigManager();
  bool init();
  void set_facade(TransporterFacade* facade) { m_facade= facade; };
  virtual void run();


  /*
    Installs subscriber that will be notified when
    config has changed
   */
  int add_config_change_subscriber(ConfigSubscriber*);

  /*
    Retrieve the current configuration in base64 packed format
   */
  bool get_packed_config(ndb_mgm_node_type nodetype,
                         BaseString& buf64, BaseString& error);

  static Config* load_config(const char* config_filename, bool mycnf,
                             BaseString& msg);

};



#endif
