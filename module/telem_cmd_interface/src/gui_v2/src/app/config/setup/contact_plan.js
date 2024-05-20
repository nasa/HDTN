import { InputTypes } from "../config_fields/utils";

const contactFields = [
    {
        name: "contact",
        label: "Contact",  
        dataType: "number", 
        inputType: InputTypes.TextField,
        required: true
    },
    {
        name: "source",
        label: "Source",  
        dataType: "number", 
        inputType: InputTypes.TextField,
        required: true
    },
    {
        name: "dest",
        label: "Destination",  
        dataType: "number", 
        inputType: InputTypes.TextField,
        required: true
    },
    {
        name: "startTime",
        label: "Start Time",  
        dataType: "number", 
        inputType: InputTypes.TextField,
        required: true
    },
    {
        name: "endTime",
        label: "End Time",  
        dataType: "number", 
        inputType: InputTypes.TextField,
        required: true
    },
    {
        name: "rateBitsPerSec",
        label: "Rate (Bits per Second)",  
        dataType: "number", 
        inputType: InputTypes.TextField,
        required: true
    }
]

export const contactPlanConfigFields = [
    {
        name: "contacts",
        label: "Contacts", 
        dataType: "array",
        arrayType: "object",
        arrayObjectType: contactFields
    }
];