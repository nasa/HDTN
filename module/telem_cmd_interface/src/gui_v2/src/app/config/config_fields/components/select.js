import InputLabel from '@mui/material/InputLabel';
import MenuItem from '@mui/material/MenuItem';
import Select from '@mui/material/Select';
import FormControl from '@mui/material/FormControl';

export default function ConfigSelect(props) {
    return (
        <FormControl fullWidth>
        <InputLabel>{props.fieldSetup.label}</InputLabel>
        <Select
            name={props.fieldSetup.name}
            value={props.value}
            label={props.fieldSetup.label}
            onChange={props.handleChange}
        >
            {props.fieldSetup.options.map((option) => (
                <MenuItem key={option.value} value={option.value}>{option.label}</MenuItem>
            ))}
        </Select>
        </FormControl>
    )
}