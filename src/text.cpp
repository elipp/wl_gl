#include "text.h"
#include "net/client.h"

mat4 text_Projection = mat4::proj_ortho(0.0, WINDOW_WIDTH, WINDOW_HEIGHT, 0.0, -1.0, 1.0);
GLuint text_texId;

ShaderProgram *text_shader = NULL;

enum { text_VAOid_log = 0, text_VAOid_inputfield = 1 };

static GLuint text_VAOids[2];
static GLuint text_shared_IBOid;
static GLuint vartracker_VAOid;

void text_set_Projection(const mat4 &m) {
	text_Projection = m;
}

static const unsigned shared_indices_count = 0xFFFF - (0xFFFF%6);

onScreenLog::InputField onScreenLog::input_field;
onScreenLog::PrintQueue onScreenLog::print_queue;

#define CURSOR_GLYPH 0x7F	// is really DEL though in utf8

float onScreenLog::pos_x = 7.0,       
      onScreenLog::pos_y = HALF_WINDOW_HEIGHT - 6;

mat4 	onScreenLog::modelview 		= mat4::identity();
GLuint 	onScreenLog::OSL_VBOid 		= 0;
float 	char_spacing_vert 		= 11.0;
float 	char_spacing_horiz 		= 7.0;
int 	onScreenLog::line_length 	= OSL_LINE_LEN;
int 	onScreenLog::num_lines_displayed = 32;
int 	onScreenLog::current_index 	= 1;	// index 0 is reserved for the overlay rectangle glyph
int 	onScreenLog::current_line_num 	= 0;
bool 	onScreenLog::visible_ 		= true;
bool 	onScreenLog::autoscroll_ 	= true;
int 	onScreenLog::num_characters_drawn = 1;

static float log_bottom_margin = char_spacing_vert*1.55;

#define IS_PRINTABLE_CHAR(c) ((c) >= 0x20 && (c) <= 0x7F)

inline GLuint texcoord_index_from_char(char c){ 
	return IS_PRINTABLE_CHAR(c) ? ((GLuint)c - 0x20) : 0;
}

inline struct xy get_glyph_xy(int index, float x, float y) {
	static const struct xy glyph_base[4] = { {0.0, 0.0}, {0.0, 12.0}, {6.0, 12.0}, {6.0, 0.0} };
	struct xy coords = { (x) + glyph_base[(index)].x, (y) + glyph_base[(index)].y };
	return coords;
}

inline glyph glyph_from_char(float x, float y, char c) { 
	glyph g;
	const unsigned tindex = texcoord_index_from_char(c);
	for (unsigned i = 0; i < 4; ++i) {
		g.vertices[i] = vertex2(get_glyph_xy(i, x, y), glyph_texcoords[tindex][i]);
	}
	return g;
}

inline glyph solid_rectangle_glyph(float upper_left_corner_x, float upper_left_corner_y, float width, float height) {
	glyph g;
	const unsigned tindex = texcoord_index_from_char(CURSOR_GLYPH); // because it has a solid color texture
	g.vertices[0] = vertex2(upper_left_corner_x, upper_left_corner_y, glyph_texcoords[tindex][0]);
	g.vertices[1] = vertex2(upper_left_corner_x, upper_left_corner_y + height, glyph_texcoords[tindex][1]);
	g.vertices[2] = vertex2(upper_left_corner_x + width, upper_left_corner_y + height, glyph_texcoords[tindex][2]);
	g.vertices[3] = vertex2(upper_left_corner_x + width, upper_left_corner_y, glyph_texcoords[tindex][3]);
	return g;
}

static GLushort *generateSharedTextIndices() {
	
	GLushort *indices = new GLushort[shared_indices_count];

	unsigned int i = 0;
	unsigned int j = 0;
	while (i < shared_indices_count) {

		indices[i] = j;
		indices[i+1] = j+1;
		indices[i+2] = j+3;
		indices[i+3] = j+1;
		indices[i+4] = j+2;
		indices[i+5] = j+3;

		i += 6;
		j += 4;
	}
	return indices;

}

GLuint generate_empty_VBO(size_t size, GLint FLAG) {

	GLuint empty_VBOid = 0;
	glGenBuffers(1, &empty_VBOid);
	glBindBuffer(GL_ARRAY_BUFFER, empty_VBOid);
	glBufferData(GL_ARRAY_BUFFER, size, NULL, FLAG);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	return empty_VBOid;

}

void onScreenLog::InputField::insert_char_to_cursor_pos(char c) {
	if (input_buffer.length() < INPUT_FIELD_BUFFER_SIZE) {
	//	if (c == VK_BACK) {
		if (c == KEY_BACKSPACE) {	// FIXME: KEY_BACKSPACE = wrong
			delete_char_before_cursor_pos();
		}
		else {
			input_buffer.insert(input_buffer.begin() + cursor_pos, c);
			move_cursor(1);
		}
	}
	refresh();
	changed_ = true;
}

void onScreenLog::InputField::refresh() {
	if (enabled_ && changed_) {
		update_VBO();
		changed_ = false;
	}
}

void onScreenLog::InputField::delete_char_before_cursor_pos() {
	if (cursor_pos < 1) { return; }
	input_buffer.erase(input_buffer.begin() + (cursor_pos-1));
	move_cursor(-1);
	changed_ = true;
}

void onScreenLog::InputField::move_cursor(int amount) {
	cursor_pos += amount;
	cursor_pos = MAX(cursor_pos, 0);
	cursor_pos = MIN((long)cursor_pos, (long)input_buffer.length());
	changed_ = true;
}

void onScreenLog::InputField::update_VBO() {

	float x_adjustment = 0;

	int i = 1;
	for (; i < input_buffer.length()+1; ++i) {
		glyph_buffer[i] = glyph_from_char(pos_x + x_adjustment, InputField::textfield_pos_y, input_buffer[i-1]);
		x_adjustment += char_spacing_horiz;
	}

	glyph_buffer[0] = glyph_from_char(pos_x + cursor_pos*char_spacing_horiz, InputField::textfield_pos_y, CURSOR_GLYPH);
	
	glBindBuffer(GL_ARRAY_BUFFER, onScreenLog::InputField::IF_VBOid);
	glBufferSubData(GL_ARRAY_BUFFER, 0, (input_buffer.length()+1)*sizeof(glyph), (const GLvoid*)&glyph_buffer[0]);

}

void onScreenLog::draw() {
	
	if (!visible_) { return; }
	
//	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glUseProgram(text_shader->program_id);
	my_glBindVertexArray(text_VAOids[text_VAOid_log]);
	
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, text_texId);
	text_shader->update_sampler2D("sampler2D_texture_color", 0);
	
	static const vec4 overlay_rect_color(0.02, 0.02, 0.02, 0.6);
	static const vec4 log_text_color(0.91, 0.91, 0.91, 1.0);

	static const mat4 overlay_modelview = mat4::identity();

	text_shader->update_uniform("mat4_Projection", (const GLvoid*)text_Projection.rawData());
	text_shader->update_uniform("mat4_ModelView", (const GLvoid*)overlay_modelview.rawData());
	text_shader->update_uniform("vec4_text_color", (const GLvoid*)overlay_rect_color.rawData());
	
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, BUFFER_OFFSET(0));

	text_shader->update_uniform("vec4_text_color", (const GLvoid*)log_text_color.rawData());
	text_shader->update_uniform("mat4_ModelView", (const GLvoid*)onScreenLog::modelview.rawData());

	glEnable(GL_SCISSOR_TEST);
	glScissor(0, (GLint)1.5*char_spacing_vert, (GLsizei) WINDOW_WIDTH, (GLsizei) num_lines_displayed * char_spacing_vert + 2);

	glDrawElements(GL_TRIANGLES, 6*(num_characters_drawn-1), GL_UNSIGNED_SHORT, BUFFER_OFFSET(6*sizeof(GLushort)));
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_BLEND);
	my_glBindVertexArray(0);

	input_field.draw();	// it's only drawn if its enabled
	
}


void onScreenLog::InputField::draw() const {

	if (!enabled_) { return; }
	glDisable(GL_DEPTH_TEST);
	
	glUseProgram(text_shader->program_id);
	my_glBindVertexArray(text_VAOids[text_VAOid_inputfield]);
	
	static const vec4 input_field_text_color(RGB_EXPAND(243, 248, 111), 1.0);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, text_texId);
	text_shader->update_sampler2D("sampler2D_texture_color", 0);
	text_shader->update_uniform("vec4_text_color", (const GLvoid*)input_field_text_color.rawData());

	static const mat4 InputField_modelview = mat4::identity();

	text_shader->update_uniform("mat4_ModelView", (const GLvoid*)InputField_modelview.rawData());
	text_shader->update_uniform("mat4_Projection", (const GLvoid*)text_Projection.rawData());
	
	glDrawElements(GL_TRIANGLES, 6*(input_buffer.length()+1), GL_UNSIGNED_SHORT, BUFFER_OFFSET(0));
	my_glBindVertexArray(0);
}

void onScreenLog::InputField::clear() {
	input_buffer.clear();
	cursor_pos = 0;
	glyph_buffer[0] = glyph_from_char(pos_x + cursor_pos*char_spacing_horiz, InputField::textfield_pos_y, CURSOR_GLYPH);
	update_VBO();
}

void onScreenLog::InputField::enable() {
	if (enabled_ == false) {
		clear();
		refresh();
	}
	enabled_ = true;
}
void onScreenLog::InputField::disable() {
	if (enabled_ == true) {
		clear();
	}
	enabled_ = false;
}

void onScreenLog::InputField::submit_and_parse() {
	LocalClient::parse_user_input(input_buffer);
	clear();
}

void onScreenLog::PrintQueue::add(const std::string &s) {
	int excess = queue.length() + s.length() - OSL_BUFFER_SIZE;
	if (excess > 0) {
		dispatch_print_queue();		
	}
	mutex.lock();
	queue.append(s);
	mutex.unlock();
}

void onScreenLog::dispatch_print_queue() {
	print_queue.mutex.lock();
	onScreenLog::print_string(print_queue.queue);
	print_queue.queue.clear();
	print_queue.mutex.unlock();
}


GLuint text_generate_shared_IBO() {
	GLuint shared_IBOid;
	GLushort *indices = generateSharedTextIndices();
	glGenBuffers(1, &shared_IBOid);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, shared_IBOid);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, shared_indices_count * sizeof(GLushort), (const GLvoid*)indices, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	
	delete [] indices;
	return shared_IBOid;
}

void onScreenLog::update_overlay_pos() {
	 glyph overlay_glyph = 
		solid_rectangle_glyph(1, 
		WINDOW_HEIGHT - ((num_lines_displayed + 2.0)*char_spacing_vert), 
		char_spacing_horiz*line_length+4, 
		char_spacing_vert*(num_lines_displayed + 5.0));
	 glBindBuffer(GL_ARRAY_BUFFER, onScreenLog::OSL_VBOid);
	 glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(overlay_glyph), &overlay_glyph);
}

int onScreenLog::init() {
	
	static const glyph overlay_glyph = 
		solid_rectangle_glyph(1, 
		WINDOW_HEIGHT - ((num_lines_displayed + 2.0)*char_spacing_vert), 
		char_spacing_horiz*line_length+4, 
		char_spacing_vert*(num_lines_displayed + 5.0));

	onScreenLog::OSL_VBOid = generate_empty_VBO(OSL_BUFFER_SIZE*sizeof(glyph), GL_DYNAMIC_DRAW);	
	text_shared_IBOid = text_generate_shared_IBO();

	my_glGenVertexArrays(2, text_VAOids);
	my_glBindVertexArray(text_VAOids[text_VAOid_log]);
	
	glEnableVertexAttribArray(TEXT_ATTRIB_POSITION);
	glEnableVertexAttribArray(TEXT_ATTRIB_TEXCOORD);

	glBindBuffer(GL_ARRAY_BUFFER, onScreenLog::OSL_VBOid);
	glVertexAttribPointer(TEXT_ATTRIB_POSITION, 2, GL_FLOAT, GL_FALSE, sizeof(struct vertex2), BUFFER_OFFSET(0));
	glVertexAttribPointer(TEXT_ATTRIB_TEXCOORD, 2, GL_FLOAT, GL_FALSE, sizeof(struct vertex2), BUFFER_OFFSET(2*sizeof(float)));

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, text_shared_IBOid);
	my_glBindVertexArray(0);
	
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(overlay_glyph), &overlay_glyph); 

	input_field.IF_VBOid = generate_empty_VBO(INPUT_FIELD_BUFFER_SIZE*sizeof(glyph), GL_DYNAMIC_DRAW);
	input_field.textfield_pos_y = WINDOW_HEIGHT - char_spacing_vert - 4;	

	my_glBindVertexArray(text_VAOids[text_VAOid_inputfield]);
	glEnableVertexAttribArray(TEXT_ATTRIB_POSITION);
	glEnableVertexAttribArray(TEXT_ATTRIB_TEXCOORD);

	glBindBuffer(GL_ARRAY_BUFFER, input_field.IF_VBOid);
	glVertexAttribPointer(TEXT_ATTRIB_POSITION, 2, GL_FLOAT, GL_FALSE, sizeof(struct vertex2), BUFFER_OFFSET(0));
	glVertexAttribPointer(TEXT_ATTRIB_TEXCOORD, 2, GL_FLOAT, GL_FALSE, sizeof(struct vertex2), BUFFER_OFFSET(2*sizeof(float)));

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, text_shared_IBOid);
	my_glBindVertexArray(0);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glDisableVertexAttribArray(TEXT_ATTRIB_POSITION);
	glDisableVertexAttribArray(TEXT_ATTRIB_TEXCOORD);

	return 1;
}

void onScreenLog::update_VBO(const std::string &buffer) {

	const size_t length = buffer.length();
	glyph *glyphs = new glyph[length];	

	int i = 0;
	static float x_adjustment = 0, y_adjustment = 0;
	static int line_beg_index = 0;

	for (i = 0; i < length; ++i) {

		char c = buffer[i];

		if (c == '\n') {
			++current_line_num;
			y_adjustment = current_line_num*char_spacing_vert;
			x_adjustment = -char_spacing_horiz;	// workaround, is incremented at the bottom of the lewp
			line_beg_index = (current_index+i);
		}

		else if (((current_index+i)-line_beg_index) >= OSL_LINE_LEN) {
			++current_line_num;
			y_adjustment = current_line_num*char_spacing_vert;
			x_adjustment = 0;
			line_beg_index = (current_index+i);
		}
	
		glyphs[i] = glyph_from_char(pos_x + x_adjustment, pos_y + y_adjustment, c);
		
		x_adjustment += char_spacing_horiz;
	}
	
	int prev_current_index = current_index;
	current_index += length;
	
	glBindBuffer(GL_ARRAY_BUFFER, onScreenLog::OSL_VBOid);
	if (current_index >= OSL_BUFFER_SIZE - 1) {
		// the excessive part will be flushed to the beginning of the VBO :P
		int excess = current_index - OSL_BUFFER_SIZE + 2;
		int fitting = buffer.length() - excess;
		glBufferSubData(GL_ARRAY_BUFFER, prev_current_index*sizeof(glyph), fitting*sizeof(glyph), (const GLvoid*)glyphs);
		glBufferSubData(GL_ARRAY_BUFFER, 1*sizeof(glyph), excess*sizeof(glyph), (const GLvoid*)(glyphs + fitting));
		current_index = excess == 0 ? 1 : excess;	// to prevent us from corrupting the overlay rectangle glyph ^^
	}
	else {
		glBufferSubData(GL_ARRAY_BUFFER, prev_current_index*sizeof(glyph), length*(sizeof(glyph)), (const GLvoid*)glyphs);
	}

	if (autoscroll_) {
		float d = (y_adjustment + pos_y + log_bottom_margin) - WINDOW_HEIGHT;
//		if (d > onScreenLog::modelview(3,1)) { 
			set_y_translation(-d);
//		}
	}
	delete [] glyphs;

}

int onScreenLog::print(const char* fmt, ...) {
	
	char *buffer = new char[OSL_BUFFER_SIZE]; 
	va_list args;
	va_start(args, fmt);

	/*SYSTEMTIME st;
    GetSystemTime(&st);*/

#ifdef _WIN32
	std::size_t msg_len = vsprintf_s(buffer, OSL_BUFFER_SIZE, fmt, args);
#elif __linux__
	std::size_t msg_len = vsnprintf(buffer, OSL_BUFFER_SIZE, fmt, args);
#endif

	msg_len = (msg_len > OSL_BUFFER_SIZE - 1) ? OSL_BUFFER_SIZE - 1 : msg_len;
	std::size_t total_len = msg_len;

	buffer[total_len] = '\0';
	buffer[OSL_BUFFER_SIZE-1] = '\0';
	va_end(args);
	
	print_queue.add(buffer);
	num_characters_drawn += total_len;
	num_characters_drawn = (num_characters_drawn > OSL_BUFFER_SIZE) ? OSL_BUFFER_SIZE : (num_characters_drawn);

	delete [] buffer;

	return num_characters_drawn;
}

void onScreenLog::print_string(const std::string &s) {
	update_VBO(s);
}

void onScreenLog::scroll(float ds) {
	float y_adjustment = current_line_num * char_spacing_vert;
	float bottom_scroll_displacement = y_adjustment + log_bottom_margin + pos_y - WINDOW_HEIGHT;
	
	onScreenLog::set_y_translation(onScreenLog::modelview(3,1) + ds);
	
	if (bottom_scroll_displacement + onScreenLog::modelview(3,1) >= 0) {
		autoscroll_ = false;

	}
	else {		
		set_y_translation(-bottom_scroll_displacement);
		autoscroll_ = true;
	}

}

void onScreenLog::set_y_translation(float new_y) {
	onScreenLog::modelview.assign(3, 1, new_y);
}

void onScreenLog::clear() {
	current_index = 1;
	current_line_num = 0;
	num_characters_drawn = 1;	// there's the overlay glyph at the beginning of the vbo =)
	onScreenLog::modelview = mat4::identity();
}


 // *** STATIC VAR DEFS FOR VARTRACKER ***

GLuint VarTracker::VT_VBOid;
std::vector<TrackableBase*> VarTracker::tracked;

static int tracker_width_chars = 36;

float VarTracker::pos_x = WINDOW_WIDTH - tracker_width_chars*char_spacing_horiz;
float VarTracker::pos_y = 7;
int VarTracker::cur_total_length = 0;
glyph VarTracker::glyph_buffer[TRACKED_MAX*TRACKED_LEN_MAX];


void VarTracker::init() {
	VarTracker::VT_VBOid = generate_empty_VBO(TRACKED_MAX * TRACKED_LEN_MAX * sizeof(glyph), GL_DYNAMIC_DRAW);
	my_glGenVertexArrays(1, &vartracker_VAOid);
	
	my_glBindVertexArray(vartracker_VAOid);
	
	glBindBuffer(GL_ARRAY_BUFFER, VarTracker::VT_VBOid);
	glEnableVertexAttribArray(ATTRIB_POSITION);
	glEnableVertexAttribArray(ATTRIB_TEXCOORD);
	glVertexAttribPointer(ATTRIB_POSITION, 2, GL_FLOAT, GL_FALSE, 16, BUFFER_OFFSET(0));
	glVertexAttribPointer(ATTRIB_TEXCOORD, 2, GL_FLOAT, GL_FALSE, 16, BUFFER_OFFSET(2*sizeof(float)));
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, text_shared_IBOid);
	
	my_glBindVertexArray(0);
}

void VarTracker::update() {
	std::string collect = "Tracked variables:\n";	// probably faster to just construct a new string  
							// every time, instead of clear()ing a static one
	static const std::string separator = "\n" + std::string(tracker_width_chars - 1, '-') + "\n";
	collect.reserve(TRACKED_MAX*TRACKED_LEN_MAX);
	for (auto &it : tracked) {
		collect += separator + it->name + ":\n" + it->print();
	}
	//collect += separator;
	
	VarTracker::update_VBO(collect);
}

void VarTracker::update_VBO(const std::string &buffer) {
	
	int i = 0;
	
	float x_adjustment = 0, y_adjustment = 0;
	int line_beg_index = 0;
	int length = buffer.length();
	int current_line_num = 0;

	for (i = 1; i < length+1; ++i) {

		char c = buffer[i-1];

		if (c == '\n') {
			++current_line_num;
			y_adjustment = current_line_num*char_spacing_vert;
			x_adjustment = -char_spacing_horiz;	// workaround, is incremented at the bottom of the lewp
			line_beg_index = i;
		}

		else if (i - line_beg_index >= tracker_width_chars) {
			++current_line_num;
			y_adjustment = current_line_num*char_spacing_vert;
			x_adjustment = 0;
			line_beg_index = i;
		}
	
		glyph_buffer[i] = glyph_from_char(VarTracker::pos_x + x_adjustment, VarTracker::pos_y + y_adjustment, c);
		x_adjustment += char_spacing_horiz;
	}		
	
	glyph_buffer[0] = solid_rectangle_glyph(VarTracker::pos_x - 5, VarTracker::pos_y - 5, tracker_width_chars * char_spacing_horiz, y_adjustment + 4*char_spacing_vert);

	VarTracker::cur_total_length = buffer.length() + 1;
	glBindBuffer(GL_ARRAY_BUFFER, VarTracker::VT_VBOid);
	glBufferSubData(GL_ARRAY_BUFFER, 0, VarTracker::cur_total_length*(sizeof(glyph)), (const GLvoid*)glyph_buffer);

}

void VarTracker::draw() {
	
	VarTracker::update();

	glDisable(GL_DEPTH_TEST);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);

	glUseProgram(text_shader->program_id);
	my_glBindVertexArray(vartracker_VAOid);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, text_texId);

	text_shader->update_sampler2D("sampler2D_texture_color", 0);
	
	static const vec4 tracker_overlay_color(0.02, 0.02, 0.02, 0.6);
	static const vec4 tracker_text_color(0.91, 0.91, 0.91, 1.0);
	
	static const mat4 tracker_modelview = mat4::identity();

	text_shader->update_uniform("mat4_Projection", (const GLvoid*)text_Projection.rawData());
	text_shader->update_uniform("vec4_text_color", (const GLvoid*)tracker_overlay_color.rawData());
	text_shader->update_uniform("mat4_ModelView", (const GLvoid*)tracker_modelview.rawData());
	
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, BUFFER_OFFSET(0));
	
	text_shader->update_uniform("vec4_text_color", (const GLvoid*)tracker_text_color.rawData());
	glDrawElements(GL_TRIANGLES, 6*(VarTracker::cur_total_length - 1), GL_UNSIGNED_SHORT, BUFFER_OFFSET(6*sizeof(GLushort)));
	my_glBindVertexArray(0);
	glDisable(GL_BLEND);
}

void VarTracker::track(TrackableBase *var) {
	auto iter = tracked.begin();

	while (iter != tracked.end()) {
		if ((*iter)->data == var->data) {
			// don't add, we already are tracking this var
			return;
		}
		++iter;
	}
	VarTracker::tracked.push_back(var);
	//onScreenLog::print("Tracker: added %s with value %s.\n", var->name.c_str(), var->print().c_str());
}

void VarTracker::untrack(const void *const data_ptr) {
	// search tracked vector for an entity with this data address
	auto iter = tracked.begin();

	while (iter != tracked.end()) {
		if ((*iter)->data == data_ptr) { 
			tracked.erase(iter);
			break;
		}
		++iter;
	}

}

void VarTracker::update_position() {
	VarTracker::pos_x = WINDOW_WIDTH - tracker_width_chars*char_spacing_horiz;
}

