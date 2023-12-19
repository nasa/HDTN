"use client";

import * as React from 'react';
import Box from '@mui/material/Box';
import Drawer from '@mui/material/Drawer';
import Toolbar from '@mui/material/Toolbar';
import List from '@mui/material/List';
import ListItem from '@mui/material/ListItem';
import ListItemButton from '@mui/material/ListItemButton';
import ListItemIcon from '@mui/material/ListItemIcon';
import ListItemText from '@mui/material/ListItemText';
import SettingsIcon from '@mui/icons-material/Settings';
import SecurityIcon from '@mui/icons-material/Security';
import CheckCircleOutlineIcon from '@mui/icons-material/CheckCircleOutline';
import Link from 'next/link';
import Snackbar from '@mui/material/Snackbar';
import { usePathname, useSearchParams } from 'next/navigation'

import ConfigTabs from './tabs';

import { generateInitialState } from './config_fields/utils';
import { hdtnConfigFields, inductsConfigFields, outductsConfigFields, storageConfigFields } from './setup/hdtn';
import { bpSecConfigFields } from './setup/bpsec';
import ConfigReviewDialog from './review_modal';

const drawerWidth = 240;

export default function HDTNConfig(props) {
  const [hdtnConfig, setHdtnConfig] = React.useState(generateInitialState(hdtnConfigFields));
  const [bpSecConfig, setBpSecConfig] = React.useState(generateInitialState(bpSecConfigFields));
  const [reviewModalOpen, setReviewModalOpen] = React.useState(false);
  const [snackbarOpen, setSnackbarOpen] = React.useState(false);
  const [snackbarMessage, setSnackbarMessage] = React.useState("");

  /* 
    Left Navigation Links
  */
  const leftNavItems = [
    {
      id: "hdtn", title: "HDTN", icon: SettingsIcon, tabs: [
        { id: "general", title: "General", configFields: hdtnConfigFields, state: hdtnConfig, setState: setHdtnConfig },
        { id: "inducts", title: "Inducts", configFields: inductsConfigFields, state: hdtnConfig, setState: setHdtnConfig, target: hdtnConfig.inductsConfig },
        { id: "outducts", title: "Outducts", configFields: outductsConfigFields, state: hdtnConfig, setState: setHdtnConfig, target: hdtnConfig.outductsConfig },
        { id: "storage", title: "Storage", configFields: storageConfigFields, state: hdtnConfig, setState: setHdtnConfig, target: hdtnConfig.storageConfig }
      ]
    },
    {
      id: "security", title: "Security (BPSec)", icon: SecurityIcon, tabs: [
        { id: "general", title: "General", configFields: bpSecConfigFields.filter((field) => field.name !== "policyRules" && field.name !== "securityFailureEventSets"), state: bpSecConfig, setState: setBpSecConfig },
        { id: "policy_rules", title: "Policy Rules", configFields: bpSecConfigFields.filter((field) => field.name === "policyRules"), state: bpSecConfig, setState: setBpSecConfig },
        { id: "failure_event_sets", title: "Failure Event Sets", configFields: bpSecConfigFields.filter((field) => field.name === "securityFailureEventSets"), state: bpSecConfig, setState: setBpSecConfig }
      ]
    },
    {
      id: "review", title: "Review", icon: CheckCircleOutlineIcon, type: "review"
    }
  ];

  const pathname = usePathname()
  const searchParams = useSearchParams()
  const currentSectionId = searchParams.get('section') ? searchParams.get('section') : leftNavItems[0].id;
  const currentSection = () => {
    for (const navItem of leftNavItems) {
      if (navItem.id === currentSectionId) {
        return navItem;
      }
    }

    return leftNavItems[0];
  }
  const currentTabId = searchParams.get('tab') ? searchParams.get('tab') : currentSection().tabs[0].id;
  const [currentNavItem, setCurrentNavItem] = React.useState(currentSection());

  const showReviewModal = () => {
    setReviewModalOpen(true);
  };

  const closeReviewModal = () => {
    setReviewModalOpen(false);
  };

  const selectReviewModalOption = (value) => {
    navigator.clipboard.writeText(value === "HDTN" ? JSON.stringify(hdtnConfig) : JSON.stringify(bpSecConfig));
    setSnackbarMessage("Succesfully copied " + value + " to your clipboard!")
    setSnackbarOpen(true);
  }

  const handleSnackbarClose = () => {
    setSnackbarOpen(false);
  }

  return (
    <>
      <Drawer
        variant="permanent"
        sx={{
          width: drawerWidth,
          flexShrink: 0,
          [`& .MuiDrawer-paper`]: { width: drawerWidth, boxSizing: 'border-box' },
        }}
      >
        <Toolbar />
        <Box sx={{ overflow: 'auto' }}>
          <List>
            {leftNavItems.map((leftNavItem) => (
              <div key={leftNavItem.id}>
                {leftNavItem.type !== "review" && <Link href={pathname + "?section=" + leftNavItem.id} onClick={() => setCurrentNavItem(leftNavItem)}>
                  <ListItem disablePadding>
                    <ListItemButton selected={leftNavItem.id === currentSectionId}>
                      <ListItemIcon>
                        <leftNavItem.icon />
                      </ListItemIcon>
                      <ListItemText primary={leftNavItem.title} />
                    </ListItemButton>
                  </ListItem>
                </Link>}
                {leftNavItem.type === "review" &&
                  <ListItem disablePadding onClick={() => showReviewModal()}>
                    <ListItemButton selected={leftNavItem.id === currentSectionId}>
                      <ListItemIcon>
                        <leftNavItem.icon />
                      </ListItemIcon>
                      <ListItemText primary={leftNavItem.title} />
                    </ListItemButton>
                  </ListItem>}
              </div>
            ))}
          </List>
        </Box>
      </Drawer>
      <Box
        component="main"
        sx={{ flexGrow: 1, bgcolor: 'background.default', p: 0 }}
      >
        <ConfigTabs tabs={currentNavItem.tabs} />
      </Box>

      {/* 
        Review Dialog (For Copying Config Files) 
      */}
      <ConfigReviewDialog
        open={reviewModalOpen}
        onClose={closeReviewModal}
        selectOption={selectReviewModalOption}
      />

      {/*
        Snackbar (For showing status messages)
      */}
      <Snackbar
        open={snackbarOpen}
        autoHideDuration={6000}
        onClose={handleSnackbarClose}
        message={snackbarMessage}
      />
    </>
  )
}