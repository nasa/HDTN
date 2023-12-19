import * as React from 'react';
import TextField from '@mui/material/TextField';

export default function ConfigTextField(props) {
    return (
        <TextField name={props.fieldSetup.name} label={props.fieldSetup.label} variant="standard" value={props.value} onChange={props.handleChange} required={props.fieldSetup.required} />
    )
}