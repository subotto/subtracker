
var REQUEST_URL = "/24ore/tracking.json";
var REQUEST_TIMEOUT = 2.0;
var REQUEST_SLEEP = 0.5;

var FPS = 30.0;

var field_width=380;
var field_height=640;
var draw;
var table;
var frames=[];
var stop = false;
var update_everything_handle = null;
var request_new_frames_handle = null;
var last_timestamp = null;
var updating_frames = false;

function cart2pol(p) {
		return [p[0],Math.sqrt(p[1]*p[1]+p[2]*p[2]), Math.atan2(p[2],p[1])];
}

function pol2cart(p) {
		return [p[0],p[1]*Math.cos(p[2]), p[1]*Math.sin(p[2])];
}

function subx2x(sx) {
		return (sx+1)*field_width/2;
}

function suby2y(sy) {
		return (sy+1)*field_height/2;
}

function create_player(draw, color) {
		var player=new Object();

		var mesh_points_cart=[
				[0,-72,-13],
				[13,-64,-13],
				[16,-51,-10],
				[16,-51,8],
				[10,-35,7],

				[10,-35,-12],
				[30,-24,-12],
				[30,24,-12],
				[30,24,	12],
				[26,31,12],
				[15,100,12],
				[15,100,-9],
				[20,102,-11],
				[20,102,16],
				[21,134,16],
				//simmetrico
				[-21,134,16],
				[-20,102,16],
				[-20,102,-11],
				[-15,100,-9],
				[-15,100,12],
				[-26,31,12],
				[-30,24,12],
				[-30,24,-12],
				[-30,-24,-12],
				[-10,-35,-12],

				[-10,-35,7],
				[-16,-51,8],
				[-16,-51,-10],
				[-13,-64,-13],
		];

		player.mesh_points=mesh_points_cart.map(cart2pol);
		player.rotated_mesh=player.mesh_points.slice();

		player.x=0;
		player.y=0;

		player.move = function(x,y){
				player.x=x;
				player.y=y;
		}

		player.angle=0.0;

		player.rotate = function(angle) {
				player.angle=angle;
				if (-Math.PI/2 <= angle && angle <= 0) {
					  player.rotated_mesh=player.mesh_points.map(function(d) {return [d[0],d[1],d[2]+player.angle];});
				}
				if (-Math.PI <= angle && angle < -Math.PI/2) {
					  player.rotated_mesh=player.mesh_points.map(function(d) {return [d[0],d[1],d[2]-(player.angle+Math.PI)];});
					  player.rotated_mesh=player.rotated_mesh.map(function(d) {return [d[0],-d[1],d[2]];});
				}
				if (0 < angle && angle <= Math.PI) {
					  player.rotate(-angle);
				}
		}

		player.group=draw.group();
		player.path=player.group.path('',true);
		player.fill=player.group.path('',true);

		player.path.attr({ fill: 'none' , stroke: '#000', 'stroke-width': 2})
		player.fill.attr({stroke: 'none', 'fill-opacity': 0.85,});
		if (color==0) {
				player.fill.attr('fill','#f00');
		}
		else {
				player.fill.attr('fill','#00f');
		}

		player.scale=0.25;

		player.draw = function() {
				var points = player.rotated_mesh.map(pol2cart);
				points = points.map(
            function(p) {
					      return [p[0]*player.scale +subx2x(player.x),p[1]*player.scale+suby2y(player.y),p[2]*player.scale];
            }
				);
				var s="M ";
				for (var i=0;i<points.length;++i) {
					  s+=points[i][0]+","+points[i][1]+" ";
				}
				s+="z"

				player.path.attr('d',s);
				player.fill.attr('d',s);

		}

		return player;
}

function create_rod(draw,player_number, offset, color) {
		var rod=[];

		for (var i=0; i<player_number;i++) {
				rod[i]=create_player(draw, color);
				rod[i].move((-(player_number-1)/2.0+i)*offset, 0);
		}

		rod.rotate=function(angle) {
				if (angle==null) return;
				for (var i=0; i<player_number;i++) {
					  rod[i].rotate(angle);
				}
		}

		rod.x=0;
		rod.y=0;

		rod.move=function(x,y) {
				dx=x-rod.x;
				dy=y-rod.y;

				rod.x=x;
				rod.y=y;

				for (var i=0; i<player_number;i++) {
					  rod[i].move(rod[i].x+dx, rod[i].y+dy);
				}
		}

		rod.offset=function(offset) {
				if (offset==null) return;
				rod.move(offset,rod.y);
		}

		rod.draw=function() {
				for (var i=0; i<player_number;i++) {
					  rod[i].draw();
				}
		}

		return rod;
}

function create_ball(draw) {
		var ball=new Object();

		ball.x=0;
		ball.y=0;

		ball.move = function(x,y) {
        ball.path.attr({ fill: '#fff', stroke: '#000' });
				if (x==null || y==null) {
            ball.path.attr({ fill: 'none', stroke: 'none' });
        }
				ball.x=x;
				ball.y=y;
		}

		ball.path=draw.circle(20);
		ball.path.attr({ fill: 'none' , stroke: 'none', 'stroke-width': 2})

		ball.draw=function() {
				ball.path.move(subx2x(ball.x)-ball.path.bbox().width/2, suby2y(ball.y)-ball.path.bbox().height/2);
		}
    ball.draw();

		return ball;
}

function create_ball_track(draw) {
	var ball_track=new Object();
	
	ball_track.positions=[];
	
	ball_track.path=draw.path('',true);
	ball_track.path.attr({ fill: 'none' , stroke: '#000', 'stroke-width': 3})
	
	ball_track.draw=function() {
			points=ball_track.positions.map(function(d){return [subx2x(d[0]), suby2y(d[1])];});
		
			var s="M ";
			for (var i=0;i<points.length;++i) {
				  s+=points[i][0]+","+points[i][1]+" ";
			}
			s+=""
			
			ball_track.path.attr('d',s);
	}
	
	ball_track.add_position=function (position) {

		if (Math.abs(position[0])<0.0001 && Math.abs(position[1])<0.0001) {
			return;
		}


		if (ball_track.positions.length > FPS*2) {
			ball_track.positions.shift();
		}
			
		ball_track.positions.push(position);
	}
	
	return ball_track;
	
}

function create_table(draw, ball_layer) {
		var table=new Object();

		table.ball_track=create_ball_track(ball_layer);
		
		table.ball = create_ball(ball_layer);

		rods_red=[];
		rods_red[0]=create_rod(draw,1,0,0);
		rods_red[0].move(0,-0.9328);
		rods_red[1]=create_rod(draw,2,0.63,0);
		rods_red[1].move(0,-0.66613);
		rods_red[2]=create_rod(draw,5,0.31,0);
		rods_red[2].move(0,-0.13323);
		rods_red[3]=create_rod(draw,3,0.558,0);
		rods_red[3].move(0,0.39968);

		rods_red.draw=function() {
				for (var i=0; i<rods_red.length;i++) {
            rods_red[i].draw();
				}
		}

		rods_blue=[];
		rods_blue[0]=create_rod(draw,1,0,1);
		rods_blue[0].move(0,+0.9328);
		rods_blue[1]=create_rod(draw,2,0.63,1);
		rods_blue[1].move(0,+0.66613);
		rods_blue[2]=create_rod(draw,5,0.31,1);
		rods_blue[2].move(0,+0.13323);
		rods_blue[3]=create_rod(draw,3,0.558,1);
		rods_blue[3].move(0,-0.39968);

		rods_blue.draw=function() {
				for (var i=0; i<rods_red.length;i++) {
					  rods_blue[i].draw();
				}
		}

		table.rods_red=rods_red;
		table.rods_blue=rods_blue;
		

		table.draw=function() {
				table.rods_red.draw();
				table.rods_blue.draw();
				table.ball.draw();
				table.ball_track.draw();
		}

		table.rotate=function(c,i,angle) {
				if (c==0)
					  table.rods_red[i].rotate(angle);
				if (c==1)
					  table.rods_blue[i].rotate(angle);
		}

		return table;
}

function display_frame(frame) {
		table.ball.move(frame.ball_y, frame.ball_x); // Sto disegnando in verticale quindi sono invertiti

		table.ball_track.add_position([frame.ball_y, frame.ball_x]);
		

		table.rods_red[0].offset(frame.rod_red_0_shift);
		table.rods_red[1].offset(frame.rod_red_1_shift);
		table.rods_red[2].offset(frame.rod_red_2_shift);
		table.rods_red[3].offset(frame.rod_red_3_shift);

		table.rods_red[0].rotate(frame.rod_red_0_angle);
		table.rods_red[1].rotate(frame.rod_red_1_angle);
		table.rods_red[2].rotate(frame.rod_red_2_angle);
		table.rods_red[3].rotate(frame.rod_red_3_angle);

		table.rods_blue[0].offset(frame.rod_blue_0_shift);
		table.rods_blue[1].offset(frame.rod_blue_1_shift);
		table.rods_blue[2].offset(frame.rod_blue_2_shift);
		table.rods_blue[3].offset(frame.rod_blue_3_shift);

		table.rods_blue[0].rotate(frame.rod_blue_0_angle);
		table.rods_blue[1].rotate(frame.rod_blue_1_angle);
		table.rods_blue[2].rotate(frame.rod_blue_2_angle);
		table.rods_blue[3].rotate(frame.rod_blue_3_angle);

		table.draw();

}

function debug(s) {
		//$("#debug_div").html($("#debug_div").html()+"<br>"+s)
    $("#debug_div").html(s)
}

function repr_frames() {
    var res = "";
    for (var i = 0; i < frames.length; i++) {
        res = res + " " + frames[i].timestamp;
    }
    return res;
}

function print_frames() {
    $("#frames_div").html(repr_frames());
}

function update_everything() {
    if (stop) return;

    var frame = frame_picker.pick_frame();

    if (frame !== null) {
        display_frame(frame);
    }
    //print_frames();
}

// A copy of QueueLengthEstimator in viewer.py
var queue_length_estimator = {

    FADING_FACTOR: 0.1,

    chunk_average: 2.0 * REQUEST_SLEEP,

    parse_frames: function(frames, ref_time, last_timestamp, queue_refill) {
        if (frames.length == 0) {
            return;
        }
        if (last_timestamp === null) {
            last_timestamp = frames[frames.length - 1].timestamp;
        }
        chunk_len = frames[frames.length - 1].timestamp - frames[0].timestamp;

        if (queue_refill) {
            return;
        }

        if (this.chunk_average === null) {
            this.chunk_average = chunk_len;
        } else {
            this.chunk_average = this.FADING_FACTOR * chunk_len + (1.0 - this.FADING_FACTOR) * this.chunk_average;
        }
    },

    get_length_estimate: function() {
        return 4.0 * this.chunk_average;
    }
}

// A copy of FramePicker in viewer.py
var frame_picker = {

    WARP_COEFF: 0.3,
    MAX_WARP_OFFSET: 0.2,
    MAX_SKIP: 1.0,

    frame_time: null,
    real_time: null,

    pick_frame: function () {
        var new_time = performance.now() / 1000.0;
        if (this.real_time === null) {
            this.real_time = new_time;
        }
        var elapsed = new_time - this.real_time;
        this.real_time = new_time;

        target_len = queue_length_estimator.get_length_estimate();
        if (frames.length > 0) {
            actual_len = frames[frames.length - 1].timestamp - frames[0].timestamp;
        } else {
            actual_len = 0.0;
        }
        var warping = 1.0 + this.WARP_COEFF * (actual_len - target_len) / target_len;
        if (warping < 1.0 - this.MAX_WARP_OFFSET) {
            warping = 1.0 - this.MAX_WARP_OFFSET;
        }
        if (warping > 1.0 + this.MAX_WARP_OFFSET) {
            warping = 1.0 + this.MAX_WARP_OFFSET;
        }

        if (this.frame_time !== null) {
            this.frame_time += elapsed * warping;
        } else {
            if (actual_len >= target_len) {
                this.frame_time = frames[frames.length - 1].timestamp - target_len;
            }
        }

        var ret = null;
        if (this.frame_time !== null) {
		        for (var i = 0; i < frames.length; i++) {
				        if (this.frame_time < frames[i].timestamp) {
                    ret = frames[i];
					          break;
                }
            }
		        frames = frames.slice(i);
		    }

        if (ret !== null && ret.timestamp > this.frame_time + this.MAX_SKIP) {
            this.frame_time = ret.timestamp;
        }

        $("#time").html("".concat("Queue length: ", actual_len, " (", frames.length, "), target length: ", target_len, ", warping: ", warping, ", frame time: ", this.frame_time, ", frame timestamp: ", ret === null ? null : ret.timestamp, "total latency: ", ret === null ? null : (new Date()).getTime() / 1000.0 - ret.timestamp));

        return ret;
    }
}

function receive_frames(response) {
    ref_time = performance.now();

    if (FPS != response.fps) {
        FPS = response.fps;
        stop_svg();
        start_svg();
    }

    var queue_refill = frames.length == 0;
    queue_length_estimator.parse_frames(response.data, ref_time, last_timestamp, queue_refill);
		frames = frames.concat(response.data);
    if (frames.length > 0) {
        last_timestamp = frames[frames.length - 1].timestamp;
    }

    updating_frames = false;
}

function request_new_frames(){
    if (stop) return;
    if (updating_frames) return;
    updating_frames = true;
		if (last_timestamp != null) {
				$.getJSON(REQUEST_URL, {'last_timestamp' : last_timestamp.toFixed(6)},
					        receive_frames);
		} else {
				$.getJSON(REQUEST_URL, {'last_timestamp' : "0"},
					        receive_frames);
		}
}

function start_svg() {

    if (update_everything_handle == null) {
        update_everything_handle = setInterval(update_everything, 1000 / FPS);
    }
    if (request_new_frames_handle == null) {
        request_new_frames_handle = setInterval(request_new_frames, 1000 * REQUEST_SLEEP);
    }

}

function stop_svg() {

    if (update_everything_handle != null) {
        clearInterval(update_everything_handle);
        update_everything_handle = null;
    }
    if (request_new_frames_handle != null) {
        clearInterval(request_new_frames_handle);
        request_new_frames_handle = null;
    }

}

function init_field() {
		draw = SVG('canvas').size(380.17, 640.18);
		ball_layer = SVG('ball_layer').size(380.17, 640.18);
		
		table = create_table(draw, ball_layer);
		table.rotate(0,0,-Math.PI/2);
		table.rotate(0,1,-Math.PI/2);
		table.rotate(0,2,-Math.PI/2);

		table.draw();

		$("#angle_slider").change(function() {
			  table.rotate(0,0,$('#angle_slider').val()/100);
			  table.rotate(0,1,$('#angle_slider').val()/100);
			  table.rotate(0,2,$('#angle_slider').val()/100);
			  table.rotate(0,3,$('#angle_slider').val()/100);
			  $('#angle_value').text($('#angle_slider').val());
			  table.draw();
		});

    start_svg();
}
