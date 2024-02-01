import * as React from 'react';
import TextField from '@mui/material/TextField';

export default function ConfigTextField(props) {
    const fieldOptions = {}
    if (props.fieldSetup.dataType === "number") {
        fieldOptions["type"] = "number";
    }

    return (
        <TextField name={props.fieldSetup.name} label={props.fieldSetup.label} variant="standard" value={props.value} onChange={props.handleChange} required={props.fieldSetup.required} {...fieldOptions}/>
    )
}