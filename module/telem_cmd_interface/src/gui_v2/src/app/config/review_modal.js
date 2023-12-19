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
import SecurityIcon from '@mui/icons-material/Security';

export default function ConfigReviewDialog(props) {
  const { onClose, selectOption, open } = props;

  const handleClose = () => {
    onClose();
  };

  return (
    <Dialog onClose={handleClose} open={open}>
      <DialogTitle>Copy HDTN Configuration Files</DialogTitle>
      <List sx={{ pt: 0 }}>
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