`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 08/14/2023 06:42:21 PM
// Design Name: 
// Module Name: axis_AD5791
// Project Name: 
// Target Devices: 
// Tool Versions: 
// Description: 
// 
// Dependencies: 
// 
// Revision:
// Revision 0.01 - File Created
// Additional Comments:
// 
//////////////////////////////////////////////////////////////////////////////////


module axis_AD5791 #(
    parameter USE_RP_DIGITAL_IO = 0,
    parameter NUM_DAC = 4,
    parameter DAC_DATA_WIDTH = 20,
    parameter DAC_WORD_WIDTH = 24,
    parameter SAXIS_TDATA_WIDTH = 32
)
(
    (* X_INTERFACE_PARAMETER = "ASSOCIATED_CLKEN a_clk" *)
    (* X_INTERFACE_PARAMETER = "ASSOCIATED_BUSIF S_AXIS1:S_AXIS2:S_AXIS3:S_AXIS4" *)
    input a_clk,
    input wire [SAXIS_TDATA_WIDTH-1:0]  S_AXIS1_tdata,
    input wire                          S_AXIS1_tvalid,
    input wire [SAXIS_TDATA_WIDTH-1:0]  S_AXIS2_tdata,
    input wire                          S_AXIS2_tvalid,
    input wire [SAXIS_TDATA_WIDTH-1:0]  S_AXIS3_tdata,
    input wire                          S_AXIS3_tvalid,
    input wire [SAXIS_TDATA_WIDTH-1:0]  S_AXIS4_tdata,
    input wire                          S_AXIS4_tvalid,

    input wire configuration_mode,
    input wire configuration_send,
    input wire [SAXIS_TDATA_WIDTH-1:0]  S_AXIS1_CFG_tdata,
    input wire                          S_AXIS1_CFG_tvalid,
    input wire [SAXIS_TDATA_WIDTH-1:0]  S_AXIS2_CFG_tdata,
    input wire                          S_AXIS2_CFG_tvalid,
    input wire [SAXIS_TDATA_WIDTH-1:0]  S_AXIS3_CFG_tdata,
    input wire                          S_AXIS3_CFG_tvalid,
    input wire [SAXIS_TDATA_WIDTH-1:0]  S_AXIS4_CFG_tdata,
    input wire                          S_AXIS4_CFG_tvalid,
    
   // inout logic [ 8-1:0] exp_p_io,
    inout  [8-1:0] exp_p_io,
    inout  [8-1:0] exp_n_io
    );
    
    
    reg PMD_clk=0;   // PMOD SPI CLK
    reg PMD_sync=1;  // PMOD SPI SYNC
    reg PMD_dac [NUM_DAC-1:0]=0; // PMOD SPI DATA DAC[1..4]
    
    reg [DAC_WORD_WIDTH-1:0] reg_dac_data[NUM_DAC-1:0]=0;
    reg [DAC_WORD_WIDTH-1:0] reg_dac_data_buf[NUM_DAC-1:0]=0;
    
    reg sync=1;
    reg start=0;
    reg [6-1:0] frame_bit_counter=0;
    
    reg [1:0] rdecii = 0;

    always @ (posedge aclk) // 120MHz
    begin
        rdecii <= rdecii+1;
        PMD_clk <= rdecii[1]; // 1/4 => 30MHz
    end

    // Latch data from AXIS when new
    always @(*)
    begin
        if (configuration_mode)
        begin
            reg_dac_data[0] <= S_AXIS1_CFG_tdata[DAC_WORD_WIDTH-1:0];
            reg_dac_data[1] <= S_AXIS2_CFG_tdata[DAC_WORD_WIDTH-1:0];
            reg_dac_data[2] <= S_AXIS3_CFG_tdata[DAC_WORD_WIDTH-1:0];
            reg_dac_data[3] <= S_AXIS4_CFG_tdata[DAC_WORD_WIDTH-1:0];
        end
        else
        begin
            reg_dac_data[0] <= {{(DAC_WORD_WIDTH-DAC_DATA_WIDTH ){1'b0}}, S_AXIS1_tdata[SAXIS_TDATA_WIDTH-1:SAXIS_TDATA_WIDTH-DAC_DATA_WIDTH]};
            reg_dac_data[1] <= {{(DAC_WORD_WIDTH-DAC_DATA_WIDTH ){1'b0}}, S_AXIS2_tdata[SAXIS_TDATA_WIDTH-1:SAXIS_TDATA_WIDTH-DAC_DATA_WIDTH]};
            reg_dac_data[2] <= {{(DAC_WORD_WIDTH-DAC_DATA_WIDTH ){1'b0}}, S_AXIS3_tdata[SAXIS_TDATA_WIDTH-1:SAXIS_TDATA_WIDTH-DAC_DATA_WIDTH]};
            reg_dac_data[3] <= {{(DAC_WORD_WIDTH-DAC_DATA_WIDTH ){1'b0}}, S_AXIS4_tdata[SAXIS_TDATA_WIDTH-1:SAXIS_TDATA_WIDTH-DAC_DATA_WIDTH]};
        end
    end


    always @(negedge PMD_clk) // spi clock
    begin
        //clkr <= 0; // generate clkr
        // Detect Frame Sync
        case (state_load_dacs)
            0: // reset mode, wait for new data
            begin
                sync <= 1;
                if (reg_dac_data_buf != reg_dac_data)
                begin
                    if (configuration_mode && configuration_send)
                    begin
                        state_load_dacs <= 1;
                    end
                    else
                    begin
                        state_load_dacs <= 1;
                    end
                end
            end
            1: // load dac start
            begin
                sync <= 1;
                frame_bit_counter <= 10'd23; // 24 bits to send
    
                // Latch Axis Data at Frame Sync Pulse and initial data serialization
                reg_dac_data_buf <= reg_dac_data;

                state_load_dacs <= 2;
            end
            2:
            begin
                sync <= 0;
                state_load_dacs <= 3; // one more sync high cycle
            end
            3:
            begin
                // completed?
                if (frame_bit_counter == 10'b0)
                begin
                    state_load_dacs <= 4; // finish
                end else
                begin
                    // next bit
                    frame_bit_counter <= frame_bit_counter - 1;
                end
            end   
            4:
            begin
                sync <= 1;
                state_load_dacs <= 0; // completed, standy next
            end   
        endcase
    end

    always @(posedge PMD_clk) // SPI transmit data setup edge
     begin
        PMD_sync   <= sync;
        PMD_dac[0] <= reg_dac_data_buf[frame_bit_counter][0];
        PMD_dac[1] <= reg_dac_data_buf[frame_bit_counter][1];
        PMD_dac[2] <= reg_dac_data_buf[frame_bit_counter][2];
        PMD_dac[3] <= reg_dac_data_buf[frame_bit_counter][3];
    end


reg [6-1:0]pass;
    
// V2.0 interface McBSP on exp_n_io[], IO-in on exp_p_io (swapped pins)
// ===========================================================================
IOBUF clk_iobuf  (.O(pass[0]),   .IO(exp_n_io[0]), .I(PMD_clk),    .T(0) );
IOBUF sync_iobuf (.O(pass[1]),   .IO(exp_n_io[1]), .I(PMD_sync),   .T(0) );
IOBUF dac0_iobuf (.O(pass[2]),   .IO(exp_n_io[2]), .I(PMD_dac[0]), .T(0) );
IOBUF dac1_iobuf (.O(pass[3]),   .IO(exp_n_io[3]), .I(PMD_dac[0]), .T(0) );
IOBUF dac2_iobuf (.O(pass[4]),   .IO(exp_n_io[4]), .I(PMD_dac[0]), .T(0) );
IOBUF dac3_iobuf (.O(pass[5]),   .IO(exp_n_io[5]), .I(PMD_dac[0]), .T(0) );

   
    
endmodule