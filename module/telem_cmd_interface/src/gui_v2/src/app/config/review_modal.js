import * as React from 'react';
import Avatar from '@mui/material/Avatar';
import List from '@mui/material/List';
import ListItem from '@mui/material/ListItem';
import ListItemAvatar from '@mui/material/ListItemAvatar';
import ListItemButton from '@mui/material/ListItemButton';
import ListItemText from '@mui/material/ListItemText';
import DialogTitle from '@mui/material/DialogTitle';
import Dialog from '@mui/material/Dialog';
import SettingsIcon from '@mui/icons-material/Settings';
import CallSplitIcon from '@mui/icons-material/CallSplit';
import SecurityIcon from '@mui/icons-material/Security';
import HubIcon from '@mui/icons-material/Hub';

export default function ConfigReviewDialog(props) {
  const { onClose, selectOption, open } = props;

  const handleClose = () => {
    onClose();
  };

  return (
    <Dialog onClose={handleClose} open={open}>
      <DialogTitle>Copy HDTN Configuration Files</DialogTitle>
      <List sx={{ pt: 0 }}>

        {/* 
          HDTN 
        */}
        <ListItem disableGutters key="hdtn">
            <ListItemButton onClick={() => selectOption("HDTN")}>
            <ListItemAvatar>
              <Avatar>
                <SettingsIcon />
              </Avatar>
            </ListItemAvatar>
            <ListItemText primary="HDTN Configuration" />
              </ListItemButton>
            </ListItem>

            {/*
              Contact Plan
            */}
            <ListItem disableGutters key="contact_plan">
            <ListItemButton onClick={() => selectOption("Contact Plan")}>
            <ListItemAvatar>
              <Avatar>
                <HubIcon />
              </Avatar>
            </ListItemAvatar>
            <ListItemText primary="Contact Plan Configuration" />
              </ListItemButton>
            </ListItem>

            {/*
              Distributed
            */}
            <ListItem disableGutters key="distributed">
            <ListItemButton onClick={() => selectOption("Distributed")}>
            <ListItemAvatar>
              <Avatar>
                <CallSplitIcon />
              </Avatar>
            </ListItemAvatar>
            <ListItemText primary="Distributed Configuration" />
              </ListItemButton>
            </ListItem>

            {/*
              BPSec
            */}
            <ListItem disableGutters key="bpsec">
            <ListItemButton onClick={() => selectOption("BPSec")}>
            <ListItemAvatar>
              <Avatar>
                <SecurityIcon />
              </Avatar>
            </ListItemAvatar>
            <ListItemText primary="Security (BPSec) Configuration" />
              </ListItemButton>
            </ListItem>
      </List>
    </Dialog>
  );
}