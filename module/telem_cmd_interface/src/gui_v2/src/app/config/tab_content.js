import * as React from 'react';
import Box from '@mui/material/Box';
import Grid from '@mui/material/Grid';

import { InputTypes, generateInitialState } from './config_fields/utils';
import ConfigField from './config_fields/field';

export default function ConfigTabContent(props) {
  const { configFields, state, setState, target } = props;

  const refreshState = () => {
    setState({ ...state })
  }

  const handleChange = (e) => {
    target[e.target.name] = e.target.value;
    refreshState();
  }

  const handleSwitchChange = (e) => {
    target[e.target.name] = e.target.checked;
    refreshState();
  }

  const handleAdd = (fieldSetup) => {
    if (fieldSetup.arrayType === "object") {
      target[fieldSetup.name] = [...target[fieldSetup.name], generateInitialState(fieldSetup.arrayObjectType)];
    } else {
      target[fieldSetup.name] = [...target[fieldSetup.name], ""];
    }
    refreshState();
  }

  const handleDelete = (fieldSetup, index) => {
    target[fieldSetup.name].splice(index, 1);
    refreshState();
  }

  return (
    <Grid container spacing={5}>
      <Grid item xs={7}>
        <Box
          component="form"
          sx={{
            '& > :not(style)': { m: 1, width: '100%' },
          }}
          autoComplete="off"
        >
          {configFields.filter((field) => field.inputType !== InputTypes.Switch).map((field) => (
            <ConfigField key={field.name} fieldSetup={field} target={target} targetKey={field.name} handleAdd={handleAdd} handleChange={handleChange} handleDelete={handleDelete} refreshState={refreshState} />
          ))}

        </Box>
      </Grid>
      <Grid item xs={5}>
        <Box
          component="form"
          sx={{
            '& > :not(style)': { m: 1, width: '100%' },
          }}
          autoComplete="off"
        >
          {configFields.filter((field) => field.inputType == InputTypes.Switch).map((field) => (
            <ConfigField key={field.name} fieldSetup={field} target={target} targetKey={field.name} handleChange={handleSwitchChange} />
          ))}
        </Box>
      </Grid>
    </Grid>
  )
}