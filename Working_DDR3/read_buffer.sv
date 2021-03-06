module read_buffer (
	input iCLK,
	READ_BUFFER.BUFFER bif, // Interface for Top level and ALU
	AVL.Master avl			  // DDR3 interface
);

parameter BUFFER_WIDTH = 124;
parameter BUFFER_SIZE = BUFFER_WIDTH*BUFFER_WIDTH;
parameter BLOCK_WIDTH = 10;
parameter BLOCK_SIZE = BLOCK_WIDTH*BLOCK_WIDTH;
parameter BLOCKS_ROW = (BUFFER_WIDTH-2)/(BLOCK_WIDTH-2);
reg [BUFFER_SIZE-1:0][31:0] buffer;

assign avl.avl_burstbegin = avl.avl_write || avl.avl_read;

//Wires connecting buffer to output blocks
wire [BLOCKS_ROW*BLOCKS_ROW-1:0][BLOCK_SIZE-1:0][31:0] blocks;
genvar j, k;
generate
	for (j=0; j < BLOCKS_ROW; j++) begin : for_j
		for (k=0; k < BLOCKS_ROW; k++) begin : for_k
			assign blocks[j*BLOCKS_ROW + k][9:0] = buffer[(0+j*8)*BUFFER_WIDTH + k*8 +: 10];
			assign blocks[j*BLOCKS_ROW + k][19:10] = buffer[(1+j*8)*BUFFER_WIDTH + k*8 +: 10];
			assign blocks[j*BLOCKS_ROW + k][29:20] = buffer[(2+j*8)*BUFFER_WIDTH + k*8 +: 10];
			assign blocks[j*BLOCKS_ROW + k][39:30] = buffer[(3+j*8)*BUFFER_WIDTH + k*8 +: 10];
			assign blocks[j*BLOCKS_ROW + k][49:40] = buffer[(4+j*8)*BUFFER_WIDTH + k*8 +: 10];
			assign blocks[j*BLOCKS_ROW + k][59:50] = buffer[(5+j*8)*BUFFER_WIDTH + k*8 +: 10];
			assign blocks[j*BLOCKS_ROW + k][69:60] = buffer[(6+j*8)*BUFFER_WIDTH + k*8 +: 10];
			assign blocks[j*BLOCKS_ROW + k][79:70] = buffer[(7+j*8)*BUFFER_WIDTH + k*8 +: 10];
			assign blocks[j*BLOCKS_ROW + k][89:80] = buffer[(8+j*8)*BUFFER_WIDTH + k*8 +: 10];
			assign blocks[j*BLOCKS_ROW + k][99:90] = buffer[(9+j*8)*BUFFER_WIDTH + k*8 +: 10];

		end
	end
endgenerate

logic [3:0] state;
logic [15:0] row;
logic [15:0] index;
logic [4:0] write_count;
logic i;

assign bif.block = blocks[bif.block_num];
// 0: idle
// 1: load from ddr
always@(posedge iCLK)
begin
	if (bif.reset) begin
		for (i = 0; i < BUFFER_SIZE; i++) buffer[i] <= 31'b0;
		write_count <= 0;
		index <= 16'b0;
		row <= 16'b0;
		state <= 0;
		bif.ready <= 1;
		avl.avl_read <= 0;
		avl.avl_write <= 0;
	end
	else begin
	case (state)
	 0 : begin
		if (avl.local_init_done && bif.load_ddr) begin
				avl.avl_address <= bif.start_address;
				state <= 1;
				bif.ready <= 0;
		end
	 end
	//Reading from DDR3 to local registers
	 1 : begin	
			avl.avl_read <= 1;

			if (!write_count[3])
				write_count <= write_count + 1'b1;

			if (avl.avl_wait_request_n) //ready to read
				state <= 2;
		end
	 2 : begin
			avl.avl_read <= 0;

			if (!write_count[3])
				write_count <= write_count + 1'b1;

			if (avl.avl_readdatavalid)
			begin 
				if (bif.pad) buffer[4*(index + 1) + BUFFER_WIDTH*(row+1) +: 4] <= avl.avl_readdata;
				else buffer[4*index + BUFFER_WIDTH*row +:4] <= avl.avl_readdata;
				state <= 3;
			end
		end
	 3 : begin
			write_count <= 5'b0;

			//Done reading
			if ((index >= bif.stride - 1) && (row >= bif.rows - 1)) state <= 4;

			else // Read next
			begin
				avl.avl_address <= avl.avl_address + 1;
				state <= 1;
				if (index >= bif.stride - 1)
				begin
					index <= 0;
					row <= row + 1;
				end else index <= index + 1;
			end
		end
	 4 : begin 
		bif.ready <= 1'b1;
		state <= 0;
	 end
	 default : state <= 0;
	 endcase
	 end
end

endmodule
