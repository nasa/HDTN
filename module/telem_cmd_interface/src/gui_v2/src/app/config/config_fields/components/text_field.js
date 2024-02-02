import * as React from 'react';
import TextField from '@mui/material/TextField';

export default function ConfigTextField(props) {
    const { fieldSetup, value, handleChange } = props;
    const fieldOptions = {};
    if (props.fieldSetup.dataType === "number") {
        fieldOptions["type"] = "number";
    }

    if (fieldSetup.required && value === "") {
        fieldOptions["error"] = true;
    }

    return (
        <TextField name={fieldSetup.name} label={fieldSetup.label} variant="outlined" value={value} onChange={handleChange} required={fieldSetup.required} {...fieldOptions} />
    )
}