import Switch from '@mui/material/Switch';
import FormControlLabel from '@mui/material/FormControlLabel';

export default function ConfigSwitch(props) {
    return (
        <FormControlLabel
          control={<Switch name={props.fieldSetup.name} checked={props.value} onChange={props.handleChange} />}
          label={props.fieldSetup.label}
          labelPlacement="start"
        />
    )
}