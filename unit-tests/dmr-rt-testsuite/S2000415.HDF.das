Attributes {
    NSCAT%20Rev%2020 {
        WVC_Lat_dim_0 {
            String name "row";
            String long_name "WVC row";
        }
        WVC_Lat_dim_1 {
            String name "WVC";
            String long_name "WVC index";
        }
        WVC_Lon_dim_0 {
            String name "row";
            String long_name "WVC row";
        }
        WVC_Lon_dim_1 {
            String name "WVC";
            String long_name "WVC index";
        }
        Num_Sigma0_dim_0 {
            String name "row";
            String long_name "WVC row";
        }
        Num_Sigma0_dim_1 {
            String name "WVC";
            String long_name "WVC index";
        }
        Num_Beam_12_dim_0 {
            String name "row";
            String long_name "WVC row";
        }
        Num_Beam_12_dim_1 {
            String name "WVC";
            String long_name "WVC index";
        }
        Num_Beam_34_dim_0 {
            String name "row";
            String long_name "WVC row";
        }
        Num_Beam_34_dim_1 {
            String name "WVC";
            String long_name "WVC index";
        }
        Num_Beam_56_dim_0 {
            String name "row";
            String long_name "WVC row";
        }
        Num_Beam_56_dim_1 {
            String name "WVC";
            String long_name "WVC index";
        }
        Num_Beam_78_dim_0 {
            String name "row";
            String long_name "WVC row";
        }
        Num_Beam_78_dim_1 {
            String name "WVC";
            String long_name "WVC index";
        }
        WVC_Quality_Flag_dim_0 {
            String name "row";
            String long_name "WVC row";
        }
        WVC_Quality_Flag_dim_1 {
            String name "WVC";
            String long_name "WVC index";
        }
        Mean_Wind_dim_0 {
            String name "row";
            String long_name "WVC row";
        }
        Mean_Wind_dim_1 {
            String name "WVC";
            String long_name "WVC index";
        }
        Wind_Speed_dim_0 {
            String name "row";
            String long_name "WVC row";
        }
        Wind_Speed_dim_1 {
            String name "WVC";
            String long_name "WVC index";
        }
        Wind_Speed_dim_2 {
            String name "position";
            String long_name "position of the ambiguities";
        }
        Wind_Dir_dim_0 {
            String name "row";
            String long_name "WVC row";
        }
        Wind_Dir_dim_1 {
            String name "WVC";
            String long_name "WVC index";
        }
        Wind_Dir_dim_2 {
            String name "position";
            String long_name "position of the ambiguities";
        }
        Error_Speed_dim_0 {
            String name "row";
            String long_name "WVC row";
        }
        Error_Speed_dim_1 {
            String name "WVC";
            String long_name "WVC index";
        }
        Error_Speed_dim_2 {
            String name "position";
            String long_name "position of the ambiguities";
        }
        Error_Dir_dim_0 {
            String name "row";
            String long_name "WVC row";
        }
        Error_Dir_dim_1 {
            String name "WVC";
            String long_name "WVC index";
        }
        Error_Dir_dim_2 {
            String name "position";
            String long_name "position of the ambiguities";
        }
        MLE_Likelihood_dim_0 {
            String name "row";
            String long_name "WVC row";
        }
        MLE_Likelihood_dim_1 {
            String name "WVC";
            String long_name "WVC index";
        }
        MLE_Likelihood_dim_2 {
            String name "position";
            String long_name "position of the ambiguities";
        }
        Num_Ambigs_dim_0 {
            String name "row";
            String long_name "WVC row";
        }
        Num_Ambigs_dim_1 {
            String name "WVC";
            String long_name "WVC index";
        }
        WVC_Lat {
            String long_name "latitude";
            String units "deg";
            Float64 scale_factor 0.01;
            Float64 scale_factor_err 0;
            Float64 add_offset 0;
            Float64 add_offset_err 0;
            Int32 calibrated_nt 22;
            Int16 valid_range -9000, 7771;
        }
        WVC_Lon {
            String long_name "longitude";
            String units "deg";
            Float64 scale_factor 0.01;
            Float64 scale_factor_err 0;
            Float64 add_offset 0;
            Float64 add_offset_err 0;
            Int32 calibrated_nt 22;
            UInt16 valid_range 0, 31242;
        }
        Num_Sigma0 {
            String long_name "The total number of sigma-0 measurements";
            String units "counts";
            Float64 scale_factor 1;
            Float64 scale_factor_err 0;
            Float64 add_offset 0;
            Float64 add_offset_err 0;
            Int32 calibrated_nt 21;
            Byte valid_range 0, 19;
        }
        Num_Beam_12 {
            String long_name "The total number of sigma-0s received from beam 1 or 2";
            String units "counts";
            Float64 scale_factor 1;
            Float64 scale_factor_err 0;
            Float64 add_offset 0;
            Float64 add_offset_err 0;
            Int32 calibrated_nt 21;
            Byte valid_range 0, 6;
        }
        Num_Beam_34 {
            String long_name "The total number of sigma-0s received from beam 3 or 4";
            String units "counts";
            Float64 scale_factor 1;
            Float64 scale_factor_err 0;
            Float64 add_offset 0;
            Float64 add_offset_err 0;
            Int32 calibrated_nt 21;
            Byte valid_range 0, 6;
        }
        Num_Beam_56 {
            String long_name "The total number of sigma-0s received from beam 5 or 6";
            String units "counts";
            Float64 scale_factor 1;
            Float64 scale_factor_err 0;
            Float64 add_offset 0;
            Float64 add_offset_err 0;
            Int32 calibrated_nt 21;
            Byte valid_range 0, 6;
        }
        Num_Beam_78 {
            String long_name "The total number of sigma-0s received from beam 7 or 8";
            String units "counts";
            Float64 scale_factor 1;
            Float64 scale_factor_err 0;
            Float64 add_offset 0;
            Float64 add_offset_err 0;
            Int32 calibrated_nt 21;
            Byte valid_range 0, 6;
        }
        WVC_Quality_Flag {
            String long_name "WVC Quality Flag";
            Float64 scale_factor 1;
            Float64 scale_factor_err 0;
            Float64 add_offset 0;
            Float64 add_offset_err 0;
            Int32 calibrated_nt 21;
            Byte valid_range 0, 3;
        }
        Mean_Wind {
            String long_name "Mean Wind Speed";
            String units "m/s";
            Float64 scale_factor 0.01;
            Float64 scale_factor_err 0;
            Float64 add_offset 0;
            Float64 add_offset_err 0;
            Int32 calibrated_nt 23;
            UInt16 valid_range 0, 2374;
        }
        Wind_Speed {
            String long_name "Wind speed associated with a WVC";
            String units "m/s";
            Float64 scale_factor 0.01;
            Float64 scale_factor_err 0;
            Float64 add_offset 0;
            Float64 add_offset_err 0;
            Int32 calibrated_nt 23;
            UInt16 valid_range 0, 2571;
        }
        Wind_Dir {
            String long_name "Wind direction solution associated with a WVC";
            String units "deg";
            Float64 scale_factor 0.01;
            Float64 scale_factor_err 0;
            Float64 add_offset 0;
            Float64 add_offset_err 0;
            Int32 calibrated_nt 23;
            UInt16 valid_range 0, 35997;
        }
        Error_Speed {
            String long_name "Uncertainty estimated for the wind speed value";
            String units "m/s";
            Float64 scale_factor 0.01;
            Float64 scale_factor_err 0;
            Float64 add_offset 0;
            Float64 add_offset_err 0;
            Int32 calibrated_nt 23;
            UInt16 valid_range 0, 2410;
        }
        Error_Dir {
            String long_name "Uncertainty estimated for the wind direction value";
            String units "deg";
            Float64 scale_factor 0.01;
            Float64 scale_factor_err 0;
            Float64 add_offset 0;
            Float64 add_offset_err 0;
            Int32 calibrated_nt 23;
            UInt16 valid_range 0, 2435;
        }
        MLE_Likelihood {
            String long_name "Relative likelihood that a given win vector solution is correct";
            Float64 scale_factor 0.1;
            Float64 scale_factor_err 0;
            Float64 add_offset 0;
            Float64 add_offset_err 0;
            Int32 calibrated_nt 22;
            Int16 valid_range -32768, 2825;
        }
        Num_Ambigs {
            String long_name "Number of ambiguous wind vectors reported for a given cell";
            String units "counts";
            Float64 scale_factor 1;
            Float64 scale_factor_err 0;
            Float64 add_offset 0;
            Float64 add_offset_err 0;
            Int32 calibrated_nt 21;
            Byte valid_range 0, 4;
        }
    }
    HDF_GLOBAL {
        String Producer_Agency "NASA\\000";
        String Producer_Institution "JPL\\000";
        String Sensor_Name "NSCAT\\000";
        String Project_ID "NSCAT\\000";
        String SIS_ID "597-512-24/1996-07-01\\000";
        String Build_ID "3.2.1/1996-11-05\\000";
        String ADEOS_Data_Package_ID "S2\\000";
        String ADEOS_Data_Package_Type "S\\000";
        String Product_Creation_Time "1996-318T01:35:16.000\\000";
        String Data_Type "L2\\000";
        String Data_Status "COMPLETE\\000";
        Int32 First_Rev_Number 415;
        String First_Rev_Eq_Crossing_Time "1996-259T04:01:28.226\\000";
        Float32 First_Rev_Eq_Crossing_Lon 279.983;
        String First_Data_Time "1996-259T03:43:48.945\\000";
        String Last_Data_Time "1996-259T05:09:48.997\\000";
        Int32 Num_Expected_Output_Records 458;
        Int32 Num_Actual_Output_Records 458;
        String Ambig_Removal_Method "Baseline used\\000";
        String HDF_Build_ID "JPL D-xxxxx 12/15/94\\000";
        String HDF_SIS_ID "JPL D-12060 12/15/94\\000";
        String HDF_Conversion_Organization "JPL PO.DAAC\\000";
        String HDF_Conversion_Time "1996-320T17:32:34       ";
        String Data_Format_Type "HDF\\000";
    }
}
