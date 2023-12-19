import * as React from 'react';
import Box from '@mui/material/Box';
import Toolbar from '@mui/material/Toolbar';
import Tabs from '@mui/material/Tabs';
import Tab from '@mui/material/Tab';

import ConfigTabPanel from './tab_panel';
import ConfigTabContent from './tab_content';

export default function ConfigTabs(props) {
    const [tabValue, setTabValue] = React.useState(0);

    React.useEffect(() => {
        setTabValue(0);
    }, [props.tabs]);

    return (
        <Box component="main" sx={{ flexGrow: 1, paddingBottom: 3, paddingTop: 1 }}>
        <Toolbar />
        <Box sx={{ borderBottom: 1, borderColor: 'divider' }}>
                <Tabs value={tabValue < props.tabs.length ? tabValue : 0} onChange={(e, newValue) => setTabValue(newValue)}>
                    {props.tabs.map((tab) => (
                        <Tab key={tab.id} label={tab.title} />
                    ))}
                </Tabs>
            </Box>

            {props.tabs.map((tab, curIndex) => (
                <ConfigTabPanel key={curIndex} value={tabValue < props.tabs.length ? tabValue : 0} index={curIndex}>
                    <Box sx={{ pl: 5, pr: 5}}>
                        <ConfigTabContent configFields={tab.configFields} state={tab.state} setState={tab.setState} target={tab.target ? tab.target: tab.state} />
                    </Box>
                </ConfigTabPanel>
            ))}
      </Box>
    )
}