import Box from '@mui/material/Box';
import Button from '@mui/material/Button';
import AddIcon from '@mui/icons-material/Add';
import ConfigField from "./field";
import Accordion from '@mui/material/Accordion';
import AccordionSummary from '@mui/material/AccordionSummary';
import AccordionDetails from '@mui/material/AccordionDetails';
import AccordionActions from '@mui/material/AccordionActions';
import ExpandMoreIcon from '@mui/icons-material/ExpandMore';
import { fieldConfigForArrayItem, generateInitialState } from './utils';

export default function ConfigArrayField(props) {
    const { fieldSetup, target, handleAdd, handleChange, handleDelete, refreshState } = props;

    const handleArrayChange = (e, field, index) => {
        if (field) {
            target[fieldSetup.name][index][field.name] = e.target.value;
        } else {
            target[fieldSetup.name][index] = e.target.value;
        }
        refreshState();
    }

    const handleArrayAdd = (field, index) => {
        if (field.arrayType === "object") {
            target[fieldSetup.name][index][field.name] = [...target[fieldSetup.name][index][field.name], generateInitialState(field.arrayObjectType)];
        } else if (field.arrayType === "string" || field.arrayType === "number") {
            target[fieldSetup.name][index][field.name] = [...target[fieldSetup.name][index][field.name], ""];
        }
        refreshState();
    }

    const handleArrayDelete = (field, innerIndex, outerIndex) => {
        target[fieldSetup.name][outerIndex][field.name].splice(innerIndex, 1)
        refreshState();
    }

    const value = target[fieldSetup.name];

    return (
        <Box>
            <p>{fieldSetup.label}</p>
            {value.map((item, curIndex) => (
                <Accordion key={curIndex} defaultExpanded>
                    <AccordionSummary
                        expandIcon={<ExpandMoreIcon />}
                        aria-controls="panel3-content"
                    >
                        {fieldSetup.arrayLabelField ? item[fieldSetup.arrayLabelField] : fieldSetup.label}
                    </AccordionSummary>
                    <AccordionDetails>
                        <Box
                            sx={{
                                '& > :not(style)': { m: 1, width: '100%' },
                            }}
                        >
                            <>
                                    {fieldSetup.arrayObjectType && fieldSetup.arrayObjectType.map((field) => (
                                        <ConfigField key={field.name} fieldSetup={field} target={item} handleAdd={() => handleArrayAdd(field, curIndex)} handleChange={(e) => handleArrayChange(e, field, curIndex)} handleDelete={(field, index) => handleArrayDelete(field, index, curIndex)} refreshState={refreshState}></ConfigField>
                                    ))}
                            </>
                            <>
                             {fieldSetup.dataType === "array" && fieldSetup.arrayType !== "object" && <ConfigField key={fieldSetup.name} fieldSetup={fieldConfigForArrayItem(fieldSetup)} target={item} handleAdd={() => handleArrayAdd(fieldSetup, curIndex)} handleChange={(e) => handleArrayChange(e, null, curIndex)} handleDelete={(fieldSetup, index) => handleArrayDelete(fieldSetup, index, curIndex)} refreshState={refreshState}></ConfigField>}
                            </>
                        </Box>
                    </AccordionDetails>
                    <AccordionActions>
                        <Button onClick={() => handleDelete(fieldSetup, curIndex)}>Delete</Button>
                    </AccordionActions>
                </Accordion>
            ))}

            <Button sx={{ mt: 3 }} variant="contained" startIcon={<AddIcon />} onClick={() => handleAdd(fieldSetup)}>
                Add {fieldSetup.label}
            </Button>
        </Box>
    )
}