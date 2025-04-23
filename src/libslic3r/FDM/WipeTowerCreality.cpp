#include "WipeTowerCreality.hpp"

#include "libslic3r/GCode/GCodeProcessor.hpp"
#include "libslic3r/Fill/FillRectilinear.hpp"
#include "libslic3r/Geometry.hpp"

#include <boost/algorithm/string/predicate.hpp>


namespace Slic3r
{

inline float align_round(float value, float base) { return std::round(value / base) * base; }

inline float align_ceil(float value, float base) { return std::ceil(value / base) * base; }

inline float align_floor(float value, float base) { return std::floor((value) / base) * base; }


class WipeTowerWriterCreality
{
public:
	WipeTowerWriterCreality(float layer_height, float line_width, GCodeFlavor flavor, const std::vector<WipeTowerCreality::FilamentParameters>& filament_parameters) :
		m_current_pos(std::numeric_limits<float>::max(), std::numeric_limits<float>::max()),
		m_current_z(0.f),
		m_current_feedrate(0.f),
		m_layer_height(layer_height),
		m_extrusion_flow(0.f),
		m_preview_suppressed(false),
		m_elapsed_time(0.f),
#if ENABLE_GCODE_VIEWER_DATA_CHECKING
        m_default_analyzer_line_width(line_width),
#endif // ENABLE_GCODE_VIEWER_DATA_CHECKING
        m_gcode_flavor(flavor),
        m_filpar(filament_parameters)
        {
            // ORCA: This class is only used by non BBL printers, so set the parameter appropriately.
            // This fixes an issue where the wipe tower was using BBL tags resulting in statistics for purging in the purge tower not being displayed.
            GCodeProcessor::s_IsBBLPrinter = false;
            // adds tag for analyzer:
            std::ostringstream str;
            str << ";" << GCodeProcessor::reserved_tag(GCodeProcessor::ETags::Height) << m_layer_height << "\n"; // don't rely on GCodeAnalyzer knowing the layer height - it knows nothing at priming
            str << ";" << GCodeProcessor::reserved_tag(GCodeProcessor::ETags::Role) << ExtrusionEntity::role_to_string(erWipeTower) << "\n";
            m_gcode += str.str();
            change_analyzer_line_width(line_width);
    }

    WipeTowerWriterCreality& change_analyzer_line_width(float line_width) {
        // adds tag for analyzer:
        std::stringstream str;
        str << ";" << GCodeProcessor::reserved_tag(GCodeProcessor::ETags::Width) << line_width << "\n";
        m_gcode += str.str();
        return *this;
    }

#if ENABLE_GCODE_VIEWER_DATA_CHECKING
    WipeTowerWriterCreality& change_analyzer_mm3_per_mm(float len, float e) {
        static const float area = float(M_PI) * 1.75f * 1.75f / 4.f;
        float mm3_per_mm = (len == 0.f ? 0.f : area * e / len);
        // adds tag for processor:
        std::stringstream str;
        str << ";" << GCodeProcessor::Mm3_Per_Mm_Tag << mm3_per_mm << "\n";
        m_gcode += str.str();
        return *this;
    }
#endif // ENABLE_GCODE_VIEWER_DATA_CHECKING

	WipeTowerWriterCreality& 			 set_initial_position(const Vec2f &pos, float width = 0.f, float depth = 0.f, float internal_angle = 0.f) {
        m_wipe_tower_width = width;
        m_wipe_tower_depth = depth;
        m_internal_angle = internal_angle;
		m_start_pos = this->rotate(pos);
		m_current_pos = pos;
		return *this;
	}

    WipeTowerWriterCreality& 			 set_position(const Vec2f &pos) { m_current_pos = pos; return *this; }

    WipeTowerWriterCreality&				 set_initial_tool(size_t tool) { m_current_tool = tool; return *this; }

	WipeTowerWriterCreality&				 set_z(float z) 
		{ m_current_z = z; return *this; }

	WipeTowerWriterCreality& 			 set_extrusion_flow(float flow)
		{ m_extrusion_flow = flow; return *this; }

	WipeTowerWriterCreality&				 set_y_shift(float shift) {
        m_current_pos.y() -= shift-m_y_shift;
        m_y_shift = shift;
        return (*this);
    }

    WipeTowerWriterCreality&            disable_linear_advance() {
        if (m_gcode_flavor == gcfRepRapSprinter || m_gcode_flavor == gcfRepRapFirmware)
            m_gcode += (std::string("M572 D") + std::to_string(m_current_tool) + " S0\n");
        else if (m_gcode_flavor == gcfKlipper)
            m_gcode += "SET_PRESSURE_ADVANCE ADVANCE=0\n";
        else
            m_gcode += "M900 K0\n";
        return *this;
    }

	// Suppress / resume G-code preview in Slic3r. Slic3r will have difficulty to differentiate the various
	// filament loading and cooling moves from normal extrusion moves. Therefore the writer
	// is asked to suppres output of some lines, which look like extrusions.
#if ENABLE_GCODE_VIEWER_DATA_CHECKING
    WipeTowerWriterCreality& suppress_preview() { change_analyzer_line_width(0.f); m_preview_suppressed = true; return *this; }
    WipeTowerWriterCreality& resume_preview() { change_analyzer_line_width(m_default_analyzer_line_width); m_preview_suppressed = false; return *this; }
#else
    WipeTowerWriterCreality& 			 suppress_preview() { m_preview_suppressed = true; return *this; }
	WipeTowerWriterCreality& 			 resume_preview()   { m_preview_suppressed = false; return *this; }
#endif // ENABLE_GCODE_VIEWER_DATA_CHECKING

	WipeTowerWriterCreality& 			 feedrate(float f)
	{
        if (f != m_current_feedrate) {
			m_gcode += "G1" + set_format_F(f) + "\n";
            m_current_feedrate = f;
        }
		return *this;
	}

	const std::string&   gcode() const { return m_gcode; }
	const std::vector<WipeTower::Extrusion>& extrusions() const { return m_extrusions; }
	float                x()     const { return m_current_pos.x(); }
	float                y()     const { return m_current_pos.y(); }
	const Vec2f& 		 pos()   const { return m_current_pos; }
	const Vec2f	 		 start_pos_rotated() const { return m_start_pos; }
	const Vec2f  		 pos_rotated() const { return this->rotate(m_current_pos); }
	float 				 elapsed_time() const { return m_elapsed_time; }
    float                get_and_reset_used_filament_length() { float temp = m_used_filament_length; m_used_filament_length = 0.f; return temp; }

	// Extrude with an explicitely provided amount of extrusion.
    WipeTowerWriterCreality& extrude_explicit(float x, float y, float e, float f = 0.f, bool record_length = false, bool limit_volumetric_flow = true)
    {
		if (x == m_current_pos.x() && y == m_current_pos.y() && e == 0.f && (f == 0.f || f == m_current_feedrate))
			// Neither extrusion nor a travel move.
			return *this;

		float dx = x - m_current_pos.x();
		float dy = y - m_current_pos.y();
        float len = std::sqrt(dx*dx+dy*dy);
        if (record_length)
            m_used_filament_length += e;

		// Now do the "internal rotation" with respect to the wipe tower center
		Vec2f rotated_current_pos(this->pos_rotated());
		Vec2f rot(this->rotate(Vec2f(x,y)));                               // this is where we want to go

        if (! m_preview_suppressed && e > 0.f && len > 0.f) {
#if ENABLE_GCODE_VIEWER_DATA_CHECKING
            change_analyzer_mm3_per_mm(len, e);
#endif // ENABLE_GCODE_VIEWER_DATA_CHECKING
            // Width of a squished extrusion, corrected for the roundings of the squished extrusions.
			// This is left zero if it is a travel move.
            float width = e * m_filpar[0].filament_area / (len * m_layer_height);
			// Correct for the roundings of a squished extrusion.
			width += m_layer_height * float(1. - M_PI / 4.);
			if (m_extrusions.empty() || m_extrusions.back().pos != rotated_current_pos)
				m_extrusions.emplace_back(WipeTower::Extrusion(rotated_current_pos, 0, m_current_tool));
			m_extrusions.emplace_back(WipeTower::Extrusion(rot, width, m_current_tool));
		}

		m_gcode += "G1";
        if (std::abs(rot.x() - rotated_current_pos.x()) > (float)EPSILON)
			m_gcode += set_format_X(rot.x());

        if (std::abs(rot.y() - rotated_current_pos.y()) > (float)EPSILON)
			m_gcode += set_format_Y(rot.y());


		if (e != 0.f)
			m_gcode += set_format_E(e);

		if (f != 0.f && f != m_current_feedrate) {
            if (limit_volumetric_flow) {
                float e_speed = e / (((len == 0.f) ? std::abs(e) : len) / f * 60.f);
                f /= std::max(1.f, e_speed / m_filpar[m_current_tool].max_e_speed);
            }
			m_gcode += set_format_F(f);
        }

        // Append newline if at least one of X,Y,E,F was changed.
        // Otherwise, remove the "G1".
        if (! boost::ends_with(m_gcode, "G1"))
            m_gcode += "\n";
        else
            m_gcode.erase(m_gcode.end()-2, m_gcode.end());

        m_current_pos.x() = x;
        m_current_pos.y() = y;

		// Update the elapsed time with a rough estimate.
        m_elapsed_time += ((len == 0.f) ? std::abs(e) : len) / m_current_feedrate * 60.f;
		return *this;
	}

    WipeTowerWriterCreality& extrude_explicit(const Vec2f &dest, float e, float f = 0.f, bool record_length = false, bool limit_volumetric_flow = true)
    { return extrude_explicit(dest.x(), dest.y(), e, f, record_length); }

    // Travel to a new XY position. f=0 means use the current value.
	WipeTowerWriterCreality& travel(float x, float y, float f = 0.f)
    { return extrude_explicit(x, y, 0.f, f); }

    WipeTowerWriterCreality& travel(const Vec2f &dest, float f = 0.f)
    { return extrude_explicit(dest.x(), dest.y(), 0.f, f); }

    // Extrude a line from current position to x, y with the extrusion amount given by m_extrusion_flow.
	WipeTowerWriterCreality& extrude(float x, float y, float f = 0.f)
	{
		float dx = x - m_current_pos.x();
		float dy = y - m_current_pos.y();
        return extrude_explicit(x, y, std::sqrt(dx*dx+dy*dy) * m_extrusion_flow, f, true);
	}

    WipeTowerWriterCreality& extrude(const Vec2f &dest, const float f = 0.f)
    { return extrude(dest.x(), dest.y(), f); }

    WipeTowerWriterCreality& rectangle(const Vec2f& ld,float width,float height,const float f = 0.f)
    {
        Vec2f corners[4];
        corners[0] = ld;
        corners[1] = ld + Vec2f(width,0.f);
        corners[2] = ld + Vec2f(width,height);
        corners[3] = ld + Vec2f(0.f,height);
        int index_of_closest = 0;
        if (x()-ld.x() > ld.x()+width-x())    // closer to the right
            index_of_closest = 1;
        if (y()-ld.y() > ld.y()+height-y())   // closer to the top
            index_of_closest = (index_of_closest==0 ? 3 : 2);

        travel(corners[index_of_closest].x(), y());      // travel to the closest corner
        travel(x(),corners[index_of_closest].y());

        int i = index_of_closest;
        do {
            ++i;
            if (i==4) i=0;
            extrude(corners[i], f);
        } while (i != index_of_closest);
        return (*this);
    }

    WipeTowerWriterCreality& rectangle(const WipeTower::box_coordinates& box, const float f = 0.f)
    {
        rectangle(Vec2f(box.ld.x(), box.ld.y()),
                  box.ru.x() - box.lu.x(),
                  box.ru.y() - box.rd.y(), f);
        return (*this);
    }

	WipeTowerWriterCreality& load(float e, float f = 0.f)
	{
		if (e == 0.f && (f == 0.f || f == m_current_feedrate))
			return *this;
		m_gcode += "G1";
		if (e != 0.f)
			m_gcode += set_format_E(e);
		if (f != 0.f && f != m_current_feedrate)
			m_gcode += set_format_F(f);
		m_gcode += "\n";
		return *this;
	}

	WipeTowerWriterCreality& retract(float e, float f = 0.f)
		{ return load(-e, f); }

	// Elevate the extruder head above the current print_z position.
    WipeTowerWriterCreality& z_hop(float hop, float f = 0.f, std::string _str = "G1")
	{ 
		m_gcode += _str + set_format_Z(m_current_z + hop);
		if (f != 0 && f != m_current_feedrate)
			m_gcode += set_format_F(f);
		m_gcode += "\n";
		return *this;
	}

    // Elevate the extruder head above the current print_z position.
    WipeTowerWriterCreality& relative_zhop(float hop, float f = 0.f, std::string _str = "G1")
    {
        m_gcode += _str + set_format_Z(hop);
        if (f != 0 && f != m_current_feedrate)
            m_gcode += set_format_F(f);
        m_gcode += "\n";
        return *this;
    }

	// Lower the extruder head back to the current print_z position.
	WipeTowerWriterCreality& z_hop_reset(float f = 0.f) 
		{ return z_hop(0, f); }

	// Move to x1, +y_increment,
	// extrude quickly amount e to x2 with feed f.
	WipeTowerWriterCreality& ram(float x1, float x2, float dy, float e0, float e, float f)
	{
        extrude_explicit(x1, m_current_pos.y() + dy, e0, f, true, false);
        extrude_explicit(x2, m_current_pos.y(), e, 0.f, true, false);
		return *this;
	}

	// Let the end of the pulled out filament cool down in the cooling tube
	// by moving up and down and moving the print head left / right
	// at the current Y position to spread the leaking material.
	WipeTowerWriterCreality& cool(float x1, float x2, float e1, float e2, float f)
	{
		extrude_explicit(x1, m_current_pos.y(), e1, f, false, false);
		extrude_explicit(x2, m_current_pos.y(), e2, false, false);
		return *this;
	}

    WipeTowerWriterCreality& set_tool(size_t tool)
	{
		m_current_tool = tool;
		return *this;
	}

	// Set extruder temperature, don't wait by default.
	WipeTowerWriterCreality& set_extruder_temp(int temperature, bool wait = false)
	{
        m_gcode += "M" + std::to_string(wait ? 109 : 104) + " S" + std::to_string(temperature) + "\n";
        return *this;
    }

    // Wait for a period of time (seconds).
	WipeTowerWriterCreality& wait(float time)
	{
        if (time==0.f)
            return *this;
        m_gcode += "G4 S" + Slic3r::float_to_string_decimal_point(time, 3) + "\n";
		return *this;
    }

	// Set speed factor override percentage.
	WipeTowerWriterCreality& speed_override(int speed)
	{
        m_gcode += "M220 S" + std::to_string(speed) + "\n";
		return *this;
    }

	// Let the firmware back up the active speed override value.
	WipeTowerWriterCreality& speed_override_backup()
    {
        // This is only supported by Prusa at this point (https://github.com/prusa3d/PrusaSlicer/issues/3114)
        if (m_gcode_flavor == gcfMarlinLegacy || m_gcode_flavor == gcfMarlinFirmware)
            m_gcode += "M220 B\n";
		return *this;
    }

	// Let the firmware restore the active speed override value.
	WipeTowerWriterCreality& speed_override_restore()
	{
        if (m_gcode_flavor == gcfMarlinLegacy || m_gcode_flavor == gcfMarlinFirmware)
            m_gcode += "M220 R\n";
		return *this;
    }

	WipeTowerWriterCreality& flush_planner_queue()
	{ 
		m_gcode += "G4 S0\n"; 
		return *this;
	}

	// Reset internal extruder counter.
	WipeTowerWriterCreality& reset_extruder()
	{ 
		m_gcode += "G92 E0\n";
		return *this;
	}

	WipeTowerWriterCreality& comment_with_value(const char *comment, int value)
    {
        m_gcode += std::string(";") + comment + std::to_string(value) + "\n";
		return *this;
    }


    WipeTowerWriterCreality& set_fan(unsigned speed)
	{
		if (speed == m_last_fan_speed)
			return *this;
		if (speed == 0)
			m_gcode += "M107\n";
        else
            m_gcode += "M106 S" + std::to_string(unsigned(255.0 * speed / 100.0)) + "\n";
		m_last_fan_speed = speed;
		return *this;
	}

	WipeTowerWriterCreality& append(const std::string& text) { m_gcode += text; return *this; }

    const std::vector<Vec2f>& wipe_path() const
    {
        return m_wipe_path;
    }

    WipeTowerWriterCreality& add_wipe_point(const Vec2f& pt)
    {
        m_wipe_path.push_back(rotate(pt));
        return *this;
    }

    WipeTowerWriterCreality& add_wipe_point(float x, float y)
    {
        return add_wipe_point(Vec2f(x, y));
    }

private:
	Vec2f         m_start_pos;
	Vec2f         m_current_pos;
    std::vector<Vec2f>  m_wipe_path;
	float    	  m_current_z;
	float 	  	  m_current_feedrate;
    size_t        m_current_tool;
	float 		  m_layer_height;
	float 	  	  m_extrusion_flow;
	bool		  m_preview_suppressed;
	std::string   m_gcode;
	std::vector<WipeTower::Extrusion> m_extrusions;
	float         m_elapsed_time;
	float   	  m_internal_angle = 0.f;
	float		  m_y_shift = 0.f;
	float 		  m_wipe_tower_width = 0.f;
	float		  m_wipe_tower_depth = 0.f;
    unsigned      m_last_fan_speed = 0;
    int           current_temp = -1;
#if ENABLE_GCODE_VIEWER_DATA_CHECKING
    const float   m_default_analyzer_line_width;
#endif // ENABLE_GCODE_VIEWER_DATA_CHECKING
    float         m_used_filament_length = 0.f;
    GCodeFlavor   m_gcode_flavor;
    const std::vector<WipeTowerCreality::FilamentParameters>& m_filpar;

	std::string   set_format_X(float x)
    {
        m_current_pos.x() = x;
        return " X" + Slic3r::float_to_string_decimal_point(x, 3);
	}

	std::string   set_format_Y(float y) {
        m_current_pos.y() = y;
        return " Y" + Slic3r::float_to_string_decimal_point(y, 3);
	}

	std::string   set_format_Z(float z) {
        return " Z" + Slic3r::float_to_string_decimal_point(z, 3);
	}

	std::string   set_format_E(float e) {
        return " E" + Slic3r::float_to_string_decimal_point(e, 4);
	}

	std::string   set_format_F(float f) {
        char buf[64];
        sprintf(buf, " F%d", int(floor(f + 0.5f)));
        m_current_feedrate = f;
        return buf;
	}

	WipeTowerWriterCreality& operator=(const WipeTowerWriterCreality &rhs);

	// Rotate the point around center of the wipe tower about given angle (in degrees)
	Vec2f rotate(Vec2f pt) const
	{
		pt.x() -= m_wipe_tower_width / 2.f;
		pt.y() += m_y_shift - m_wipe_tower_depth / 2.f;
	    double angle = m_internal_angle * float(M_PI/180.);
	    double c = cos(angle);
	    double s = sin(angle);
	    return Vec2f(float(pt.x() * c - pt.y() * s) + m_wipe_tower_width / 2.f, float(pt.x() * s + pt.y() * c) + m_wipe_tower_depth / 2.f);
	}

}; // class WipeTowerWriterCreality



WipeTower::ToolChangeResult WipeTowerCreality::construct_tcr(
    WipeTowerWriterCreality& writer, bool priming, size_t old_tool, bool is_finish) const
{
    WipeTower::ToolChangeResult result;
    result.priming      = priming;
    result.initial_tool = int(old_tool);
    result.new_tool     = int(m_current_tool);
    result.print_z      = m_z_pos;
    result.layer_height = m_layer_height;
    result.elapsed_time = writer.elapsed_time();
    result.start_pos    = writer.start_pos_rotated();
    result.end_pos      = priming ? writer.pos() : writer.pos_rotated();
    result.gcode        = std::move(writer.gcode());
#if ORCA_CHECK_GCODE_PLACEHOLDERS
    result.gcode += is_finish ? ";toolchange_change finished ################\n" : ";toolchange_change ################\n";
#endif

    result.extrusions   = std::move(writer.extrusions());
    result.wipe_path    = std::move(writer.wipe_path());
    result.is_finish_first = is_finish;
    return result;
}



WipeTowerCreality::WipeTowerCreality(const PrintConfig& config, const PrintRegionConfig& default_region_config,int plate_idx, Vec3d plate_origin, const std::vector<std::vector<float>>& wiping_matrix, size_t initial_tool) :
    m_wipe_tower_pos(config.wipe_tower_x.get_at(plate_idx), config.wipe_tower_y.get_at(plate_idx)),
    m_wipe_tower_width(float(config.prime_tower_width)),
    m_wipe_tower_rotation_angle(float(config.wipe_tower_rotation_angle)),
    m_wipe_tower_brim_width(float(config.prime_tower_brim_width)),
    m_wipe_tower_cone_angle(float(config.wipe_tower_cone_angle)),
    m_extra_spacing(float(config.wipe_tower_extra_spacing/100.)),
    m_y_shift(0.f),
    m_z_pos(0.f), 
    m_z_offset(config.z_offset),
    m_bridging(float(config.wipe_tower_bridging)),
    m_no_sparse_layers(config.wipe_tower_no_sparse_layers),
    m_gcode_flavor(config.gcode_flavor),
    m_travel_speed(config.travel_speed),
    m_infill_speed(default_region_config.sparse_infill_speed),
    m_perimeter_speed(default_region_config.inner_wall_speed),
    m_current_tool(initial_tool),
    wipe_volumes(wiping_matrix),
    m_wipe_tower_max_purge_speed(float(config.wipe_tower_max_purge_speed)),
    m_enable_timelapse_print(config.timelapse_type.value == TimelapseType::tlSmooth)
{
    // Read absolute value of first layer speed, if given as percentage,
    // it is taken over following default. Speeds from config are not
    // easily accessible here.
    const float default_speed = 60.f;
    m_first_layer_speed = config.initial_layer_speed;
    if (m_first_layer_speed == 0.f) // just to make sure autospeed doesn't break it.
        m_first_layer_speed = default_speed / 2.f;

    // Autospeed may be used...
    if (m_infill_speed == 0.f)
        m_infill_speed = 80.f;
    if (m_perimeter_speed == 0.f)
        m_perimeter_speed = 80.f;

    // Calculate where the priming lines should be - very naive test not detecting parallelograms etc.
    const std::vector<Vec2d>& bed_points = config.printable_area.values;
    BoundingBoxf bb(bed_points);
    m_bed_width = float(bb.size().x());
    m_bed_shape = (bed_points.size() == 4 ? RectangularBed : CircularBed);

    if (m_bed_shape == CircularBed) {
        // this may still be a custom bed, check that the points are roughly on a circle
        double r2 = std::pow(m_bed_width/2., 2.);
        double lim2 = std::pow(m_bed_width/10., 2.);
        Vec2d center = bb.center();
        for (const Vec2d& pt : bed_points)
            if (std::abs(std::pow(pt.x()-center.x(), 2.) + std::pow(pt.y()-center.y(), 2.) - r2) > lim2) {
                m_bed_shape = CustomBed;
                break;
            }
    }

    m_bed_bottom_left = m_bed_shape == RectangularBed
                  ? Vec2f(bed_points.front().x(), bed_points.front().y())
                  : Vec2f::Zero();

    m_prime_tower_enhance_type = config.prime_tower_enhance_type;
}



void WipeTowerCreality::set_extruder(size_t idx, const PrintConfig& config)
{
    //while (m_filpar.size() < idx+1)   // makes sure the required element is in the vector
    m_filpar.push_back(FilamentParameters());

    m_filpar[idx].material = config.filament_type.get_at(idx);
    m_filpar[idx].is_soluble = config.filament_soluble.get_at(idx);
    m_filpar[idx].temperature = config.nozzle_temperature.get_at(idx);
    m_filpar[idx].first_layer_temperature = config.nozzle_temperature_initial_layer.get_at(idx);

    m_filpar[idx].filament_area = float((M_PI/4.f) * pow(config.filament_diameter.get_at(idx), 2)); // all extruders are assumed to have the same filament diameter at this point
    float nozzle_diameter = float(config.nozzle_diameter.get_at(idx));
    m_filpar[idx].nozzle_diameter = nozzle_diameter; // to be used in future with (non-single) multiextruder MM

    float max_vol_speed = float(config.filament_max_volumetric_speed.get_at(idx));
    if (max_vol_speed!= 0.f)
        m_filpar[idx].max_e_speed = (max_vol_speed / filament_area());

    m_perimeter_width = nozzle_diameter * Width_To_Nozzle_Ratio; // all extruders are now assumed to have the same diameter

    {
        std::istringstream stream{config.filament_ramming_parameters.get_at(idx)};
        float speed = 0.f;
        stream >> m_filpar[idx].ramming_line_width_multiplicator >> m_filpar[idx].ramming_step_multiplicator;
        m_filpar[idx].ramming_line_width_multiplicator /= 100;
        m_filpar[idx].ramming_step_multiplicator /= 100;
        while (stream >> speed)
            m_filpar[idx].ramming_speed.push_back(speed);
        // ramming_speed now contains speeds to be used for every 0.25s piece of the ramming line.
        // This allows to have the ramming flow variable. The 0.25s value is how it is saved in config
        // and the same time step has to be used when the ramming is performed.
    }

    m_used_filament_length.resize(std::max(m_used_filament_length.size(), idx + 1)); // makes sure that the vector is big enough so we don't have to check later
}

WipeTower::ToolChangeResult WipeTowerCreality::tool_change(size_t tool, bool extrude_perimeter, bool first_toolchange_to_nonsoluble)
{
    size_t old_tool = m_current_tool;

    float wipe_area = 0.f;
	float wipe_volume = 0.f;
	// Finds this toolchange info
	if (tool != (unsigned int)(-1))
	{
		for (const auto &b : m_layer_info->tool_changes)
			if ( b.new_tool == tool ) {
                wipe_volume = b.wipe_volume;
				wipe_area = b.required_depth * m_layer_info->extra_spacing;
				break;
			}
	}
	else {
		// Otherwise we are going to Unload only. And m_layer_info would be invalid.
	}

    WipeTower::box_coordinates cleaning_box(
		Vec2f(m_perimeter_width / 2.f, m_perimeter_width / 2.f),
		m_wipe_tower_width - m_perimeter_width,
        (tool != (unsigned int)(-1) ? wipe_area+m_depth_traversed-0.5f*m_perimeter_width
                                    : m_wipe_tower_depth-m_perimeter_width));

	WipeTowerWriterCreality writer(m_layer_height, m_perimeter_width, m_gcode_flavor, m_filpar);
	writer.set_extrusion_flow(m_extrusion_flow)
		.set_z(m_z_pos)
		.set_initial_tool(m_current_tool)
        .set_y_shift(m_y_shift + (tool!=(unsigned int)(-1) && (m_current_shape == SHAPE_REVERSED) ? m_layer_info->depth - m_layer_info->toolchanges_depth(): 0.f))
		.append(";--------------------\n"
				"; CP TOOLCHANGE START\n")
		.comment_with_value(" toolchange #", m_num_tool_changes + 1); // the number is zero-based

    if (tool != (unsigned)(-1)){
        writer.append(std::string("; material : " + (m_current_tool < m_filpar.size() ? m_filpar[m_current_tool].material : "(NONE)") + " -> " + m_filpar[tool].material + "\n").c_str())
            .append(";--------------------\n");
        writer.append(";" + GCodeProcessor::reserved_tag(GCodeProcessor::ETags::Wipe_Tower_Start) + "\n");
    }

    writer.speed_override_backup();
	writer.speed_override(100);

	Vec2f initial_position = cleaning_box.ld + Vec2f(0.f, m_depth_traversed);
    writer.set_initial_position(initial_position, m_wipe_tower_width, m_wipe_tower_depth, m_internal_rotation);

    // Ram the hot material out of the melt zone, retract the filament into the cooling tubes and let it cool.
    if (tool != (unsigned int)-1){ 			// This is not the last change.
        toolchange_Unload(writer, cleaning_box, m_filpar[m_current_tool].material,
                          is_first_layer() ? m_filpar[tool].first_layer_temperature : m_filpar[tool].temperature);
        toolchange_Change(writer, tool, m_filpar[tool].material); // Change the tool, set a speed override for soluble and flex materials.

        writer.travel(writer.x(), writer.y()-m_perimeter_width); // cooling and loading were done a bit down the road
        /* if (extrude_perimeter) {
            WipeTower::box_coordinates wt_box(Vec2f(0.f, (m_current_shape == SHAPE_REVERSED) ? m_layer_info->toolchanges_depth() - m_layer_info->depth :
                                                                                    0.f),
                                   m_wipe_tower_width, m_layer_info->depth + m_perimeter_width);
            // align the perimeter
            wt_box = align_perimeter(wt_box);
            writer.rectangle(wt_box);
            writer.travel(initial_position);
        }

        if (first_toolchange_to_nonsoluble) {
            writer.travel(Vec2f(0, 0));
            writer.travel(initial_position);
        }*/

        toolchange_Wipe(writer, cleaning_box, wipe_volume);     // Wipe the newly loaded filament until the end of the assigned wipe area.
        writer.append(";" + GCodeProcessor::reserved_tag(GCodeProcessor::ETags::Wipe_Tower_End) + "\n");
        ++ m_num_tool_changes;
    } else
        toolchange_Unload(writer, cleaning_box, m_filpar[m_current_tool].material, m_filpar[m_current_tool].temperature);

    m_depth_traversed += wipe_area;

	writer.speed_override_restore();
    writer.feedrate(m_travel_speed * 60.f)
          .flush_planner_queue()
          .reset_extruder()
          .append("; CP TOOLCHANGE END\n"
                  ";------------------\n"
                  "\n\n");

    // Ask our writer about how much material was consumed:
    if (m_current_tool < m_used_filament_length.size())
        m_used_filament_length[m_current_tool] += writer.get_and_reset_used_filament_length();

    //return construct_tcr(writer, old_tool, false);
    return construct_tcr(writer, false, old_tool, false);
}


// Ram the hot material out of the melt zone, retract the filament into the cooling tubes and let it cool.
void WipeTowerCreality::toolchange_Unload(
	WipeTowerWriterCreality &writer,
	const WipeTower::box_coordinates 	&cleaning_box,
	const std::string&		 current_material,
	const int 				 new_temperature)
{
	float xl = cleaning_box.ld.x() + 1.f * m_perimeter_width;
	float xr = cleaning_box.rd.x() - 1.f * m_perimeter_width;

    const float line_width = m_perimeter_width * m_filpar[m_current_tool].ramming_line_width_multiplicator;       // desired ramming line thickness
	const float y_step = line_width * m_filpar[m_current_tool].ramming_step_multiplicator * m_extra_spacing; // spacing between lines in mm

    const Vec2f ramming_start_pos = Vec2f(xl, cleaning_box.ld.y() + m_depth_traversed + y_step/2.f);

    writer.append("; CP TOOLCHANGE UNLOAD\n")
        .change_analyzer_line_width(line_width);

	unsigned i = 0;										// iterates through ramming_speed
	m_left_to_right = true;								// current direction of ramming
	float remaining = xr - xl ;							// keeps track of distance to the next turnaround
	float e_done = 0;									// measures E move done from each segment   

    writer.set_position(ramming_start_pos);

	Vec2f end_of_ramming(writer.x(),writer.y());
    writer.change_analyzer_line_width(m_perimeter_width);   // so the next lines are not affected by ramming_line_width_multiplier

    // Retraction:
    float old_x = writer.x();
    float turning_point = (!m_left_to_right ? xl : xr );
    // Wipe tower should only change temperature with single extruder MM. Otherwise, all temperatures should
    // be already set and there is no need to change anything. Also, the temperature could be changed
    // for wrong extruder.
    {
        if (new_temperature != 0 && (new_temperature != m_old_temperature || is_first_layer()) ) { 	// Set the extruder temperature, but don't wait.
            // If the required temperature is the same as last time, don't emit the M104 again (if user adjusted the value, it would be reset)
            // However, always change temperatures on the first layer (this is to avoid issues with priming lines turned off).
            writer.set_extruder_temp(new_temperature, false);
            m_old_temperature = new_temperature;
        }
    }
    
    // this is to align ramming and future wiping extrusions, so the future y-steps can be uniform from the start:
    // the perimeter_width will later be subtracted, it is there to not load while moving over just extruded material
    Vec2f pos = Vec2f(end_of_ramming.x(), end_of_ramming.y() + (y_step/m_extra_spacing-m_perimeter_width) / 2.f + m_perimeter_width);
    writer.set_position(pos);

    writer.resume_preview()
          .flush_planner_queue();
}

// Change the tool, set a speed override for soluble and flex materials.
void WipeTowerCreality::toolchange_Change(
	WipeTowerWriterCreality &writer,
    const size_t 	new_tool,
    const std::string&  new_material)
{
#if ORCA_CHECK_GCODE_PLACEHOLDERS
    writer.append("; CP TOOLCHANGE CHANGE \n");
#endif
    // Ask the writer about how much of the old filament we consumed:
    if (m_current_tool < m_used_filament_length.size())
    	m_used_filament_length[m_current_tool] += writer.get_and_reset_used_filament_length();

    // This is where we want to place the custom gcodes. We will use placeholders for this.
    // These will be substituted by the actual gcodes when the gcode is generated.
    writer.append("[change_filament_gcode]\n");
    //std::string z_up_for_firmware = "[z_up_for_firmware]"
 
     //writer.z_hop(0.4f + m_z_offset, 1200.0f, "G0"); // �̼�bug,��ʱ̧��0.4,��ֹZ�߶ȴ����µ��ƶ���������ʱ�����в�
     writer.relative_zhop(0.4f + m_z_offset, 1200.0f, "relative_zhop_up_for_firmware G0"); // �̼�bug,��ʱ̧��0.4,��ֹZ�߶ȴ����µ��ƶ���������ʱ�����в�
    // Travel to where we assume we are. Custom toolchange or some special T code handling (parking extruder etc)
    // gcode could have left the extruder somewhere, we cannot just start extruding. We should also inform the
    // postprocessor that we absolutely want to have this in the gcode, even if it thought it is the same as before.

    Vec2f current_pos = writer.pos_rotated();
    writer.feedrate(m_travel_speed * 60.f) // see https://github.com/prusa3d/PrusaSlicer/issues/5483
          .append(std::string("G1 X") + Slic3r::float_to_string_decimal_point(current_pos.x())
                             +  " Y"  + Slic3r::float_to_string_decimal_point(current_pos.y())
                             + WipeTower::never_skip_tag() + "\n");


     writer.relative_zhop(m_z_offset,0.0,"relative_zhop_recovery_for_firmware G1");
  //   writer.z_hop(m_z_offset);
    writer.append("[deretraction_from_wipe_tower_generator]");

     // Orca TODO: handle multi extruders
    // The toolchange Tn command will be inserted later, only in case that the user does
    // not provide a custom toolchange gcode.
	writer.set_tool(new_tool); // This outputs nothing, the writer just needs to know the tool has changed.
    // writer.append("[filament_start_gcode]\n");


	writer.flush_planner_queue();
	m_current_tool = new_tool;
}

// Wipe the newly loaded filament until the end of the assigned wipe area.
void WipeTowerCreality::toolchange_Wipe(
	WipeTowerWriterCreality &writer,
	const WipeTower::box_coordinates  &cleaning_box,
	float wipe_volume)
{
	// Increase flow on first layer, slow down print.
    writer.set_extrusion_flow(m_extrusion_flow * (is_first_layer() ? 1.18f : 1.f))
		  .append("; CP TOOLCHANGE WIPE\n");
	const float& xl = cleaning_box.ld.x();
	const float& xr = cleaning_box.rd.x();

	// Variables x_to_wipe and traversed_x are here to be able to make sure it always wipes at least
    //   the ordered volume, even if it means violating the box. This can later be removed and simply
    // wipe until the end of the assigned area.

	float x_to_wipe = volume_to_length(wipe_volume, m_perimeter_width, m_layer_height) * (is_first_layer() ? m_extra_spacing : 1.f);
	float dy = (is_first_layer() ? 1.f : m_extra_spacing) * m_perimeter_width; // Don't use the extra spacing for the first layer.
    // All the calculations in all other places take the spacing into account for all the layers.

	// If spare layers are excluded->if 1 or less toolchange has been done, it must be sill the first layer, too.So slow down.
    const float target_speed = is_first_layer() || (m_num_tool_changes <= 1 && m_no_sparse_layers) ? m_first_layer_speed * 60.f : std::min(m_wipe_tower_max_purge_speed * 60.f, m_infill_speed * 60.f);
    float wipe_speed = 0.33f * target_speed;

    // if there is less than 2.5*m_perimeter_width to the edge, advance straightaway (there is likely a blob anyway)
    if ((m_left_to_right ? xr-writer.x() : writer.x()-xl) < 2.5f*m_perimeter_width) {
        writer.travel((m_left_to_right ? xr-m_perimeter_width : xl+m_perimeter_width),writer.y()+dy);
        m_left_to_right = !m_left_to_right;
    }
    
    // now the wiping itself:
	for (int i = 0; true; ++i)	{
		if (i!=0) {
            if      (wipe_speed < 0.34f * target_speed) wipe_speed = 0.375f * target_speed;
            else if (wipe_speed < 0.377 * target_speed) wipe_speed = 0.458f * target_speed;
            else if (wipe_speed < 0.46f * target_speed) wipe_speed = 0.875f * target_speed;
            else wipe_speed = std::min(target_speed, wipe_speed + 50.f);
		}

		float traversed_x = writer.x();
		if (m_left_to_right)
            writer.extrude(xr - 0.5f * m_perimeter_width, writer.y(), wipe_speed);
		else
            writer.extrude(xl + 0.5f * m_perimeter_width, writer.y(), wipe_speed);

        if (writer.y()+float(EPSILON) > cleaning_box.lu.y()-0.5f*m_perimeter_width)
            break;		// in case next line would not fit

		traversed_x -= writer.x();
        x_to_wipe -= std::abs(traversed_x);
		if (x_to_wipe < WT_EPSILON) {
			break;
		}
		// stepping to the next line:
        writer.extrude(writer.x(), writer.y() + dy);
		m_left_to_right = !m_left_to_right;
	}

    // We may be going back to the model - wipe the nozzle. If this is followed
    // by finish_layer, this wipe path will be overwritten.
    writer.add_wipe_point(writer.x(), writer.y())
          .add_wipe_point(writer.x(), writer.y() - dy)
          .add_wipe_point(! m_left_to_right ? m_wipe_tower_width : 0.f, writer.y() - dy);

    if (m_layer_info != m_plan.end() && m_current_tool != m_layer_info->tool_changes.back().new_tool)
        m_left_to_right = !m_left_to_right;

    writer.set_extrusion_flow(m_extrusion_flow); // Reset the extrusion flow.
}


// BBS
WipeTower::box_coordinates WipeTowerCreality::align_perimeter(const WipeTower::box_coordinates& perimeter_box)
{
    WipeTower::box_coordinates aligned_box = perimeter_box;

    float spacing = m_extra_spacing * m_perimeter_width;
    float up      = perimeter_box.lu(1) - m_perimeter_width;
    up            = align_ceil(up, spacing);
    up += m_perimeter_width;
    up = std::min(up, m_wipe_tower_depth);

    float down = perimeter_box.ld(1) - m_perimeter_width;
    down       = align_floor(down, spacing);
    down += m_perimeter_width;
    down = std::max(down, -m_y_shift);

    aligned_box.lu(1) = aligned_box.ru(1) = up;
    aligned_box.ld(1) = aligned_box.rd(1) = down;

    return aligned_box;
}

WipeTower::ToolChangeResult WipeTowerCreality::finish_layer(bool extrude_perimeter, bool extruder_fill)
{
	assert(! this->layer_finished());
    m_current_layer_finished = true;

    size_t old_tool = m_current_tool;

	WipeTowerWriterCreality writer(m_layer_height, m_perimeter_width, m_gcode_flavor, m_filpar);
	writer.set_extrusion_flow(m_extrusion_flow)
		.set_z(m_z_pos)
		.set_initial_tool(m_current_tool)
        .set_y_shift(m_y_shift - (m_current_shape == SHAPE_REVERSED ? m_layer_info->toolchanges_depth() : 0.f));


	// Slow down on the 1st layer.
    // If spare layers are excluded -> if 1 or less toolchange has been done, it must be still the first layer, too. So slow down.
    bool first_layer = is_first_layer() || (m_num_tool_changes <= 1 && m_no_sparse_layers);
    float                      feedrate      = first_layer ? m_first_layer_speed * 60.f : std::min(m_wipe_tower_max_purge_speed * 60.f, m_infill_speed * 60.f);
    float current_depth = m_layer_info->depth - m_layer_info->toolchanges_depth();
    WipeTower::box_coordinates fill_box(Vec2f(m_perimeter_width, m_layer_info->depth-(current_depth-m_perimeter_width)),
                             m_wipe_tower_width - 2 * m_perimeter_width, current_depth-m_perimeter_width);


    writer.set_initial_position((m_left_to_right ? fill_box.ru : fill_box.lu), // so there is never a diagonal travel
                                 m_wipe_tower_width, m_wipe_tower_depth, m_internal_rotation);

    bool toolchanges_on_layer = m_layer_info->toolchanges_depth() > WT_EPSILON;
    WipeTower::box_coordinates wt_box(Vec2f(0.f, (m_current_shape == SHAPE_REVERSED ? m_layer_info->toolchanges_depth() : 0.f)),
                        m_wipe_tower_width, m_layer_info->depth + m_perimeter_width);

    // inner perimeter of the sparse section, if there is space for it:
    if (fill_box.ru.y() - fill_box.rd.y() > m_perimeter_width - WT_EPSILON)
        writer.rectangle(fill_box.ld, fill_box.rd.x()-fill_box.ld.x(), fill_box.ru.y()-fill_box.rd.y(), feedrate);

    // we are in one of the corners, travel to ld along the perimeter:
    if (writer.x() > fill_box.ld.x()+EPSILON) writer.travel(fill_box.ld.x(),writer.y());
    if (writer.y() > fill_box.ld.y()+EPSILON) writer.travel(writer.x(),fill_box.ld.y());

    // Extrude infill to support the material to be printed above.
    const float dy = (fill_box.lu.y() - fill_box.ld.y() - m_perimeter_width);
    float left = fill_box.lu.x() + 2*m_perimeter_width;
    float right = fill_box.ru.x() - 2 * m_perimeter_width;
    //if (extruder_fill && dy > m_perimeter_width)
      if ( dy > m_perimeter_width)
    {
        writer.travel(fill_box.ld + Vec2f(m_perimeter_width * 2, 0.f))
              .append(";--------------------\n"
                      "; CP EMPTY GRID START\n")
              .comment_with_value(wipe_tower_layer_change_tag, m_num_layer_changes + 1);

        // Is there a soluble filament wiped/rammed at the next layer?
        // If so, the infill should not be sparse.
        bool solid_infill = m_layer_info+1 == m_plan.end()
                          ? false
                          : std::any_of((m_layer_info+1)->tool_changes.begin(),
                                        (m_layer_info+1)->tool_changes.end(),
                                   [this](const WipeTowerInfo::ToolChange& tch) {
                                       return m_filpar[tch.new_tool].is_soluble
                                           || m_filpar[tch.old_tool].is_soluble;
                                   });
        solid_infill |= first_layer && m_adhesion;

        if (solid_infill) {
            float sparse_factor = 1.5f; // 1=solid, 2=every other line, etc.
            if (first_layer) { // the infill should touch perimeters
                left  -= m_perimeter_width;
                right += m_perimeter_width;
                sparse_factor = 1.f;
            }
            float y = fill_box.ld.y() + m_perimeter_width;
            int n = dy / (m_perimeter_width * sparse_factor);
            float spacing = (dy-m_perimeter_width)/(n-1);
            int i=0;
            for (i=0; i<n; ++i) {
                writer.extrude(writer.x(), y, feedrate)
                      .extrude(i%2 ? left : right, y);
                y = y + spacing;
            }
            writer.extrude(writer.x(), fill_box.lu.y());
        } else {
            // Extrude an inverse U at the left of the region and the sparse infill.
            writer.extrude(fill_box.lu + Vec2f(m_perimeter_width * 2, 0.f), feedrate);

            const int n = 1+int((right-left)/m_bridging);
            const float dx = (right-left)/n;
            for (int i=1;i<=n;++i) {
                float x=left+dx*i;
                writer.travel(x,writer.y());
                writer.extrude(x,i%2 ? fill_box.rd.y() : fill_box.ru.y());
            }
        }

        writer.append("; CP EMPTY GRID END\n"
                      ";------------------\n\n\n\n\n\n\n");
    }

    // outer perimeter (always):
    /* WipeTower::box_coordinates wt_box(Vec2f(0.f, (m_current_shape == SHAPE_REVERSED ? m_layer_info->toolchanges_depth() : 0.f)),
                                      m_wipe_tower_width, m_layer_info->depth + m_perimeter_width);
    wt_box = this->align_perimeter(wt_box);
    if (extrude_perimeter) {
        writer.rectangle(wt_box, feedrate);
    }*/

    const float spacing = m_perimeter_width - m_layer_height*float(1.-M_PI_4);

    // This block creates the stabilization cone.
    // First define a lambda to draw the rectangle with stabilization.
    auto supported_rectangle = [this, &writer, spacing](const WipeTower::box_coordinates& wt_box, double feedrate, bool infill_cone) -> Polygon {
        const auto [R, support_scale] = WipeTower2::get_wipe_tower_cone_base(m_wipe_tower_width, m_wipe_tower_height, m_wipe_tower_depth, m_wipe_tower_cone_angle);

        double z = m_no_sparse_layers ? (m_current_height + m_layer_info->height) : m_layer_info->z; // the former should actually work in both cases, but let's stay on the safe side (the 2.6.0 is close)

        double r = std::tan(Geometry::deg2rad(m_wipe_tower_cone_angle/2.f)) * (m_wipe_tower_height - z);
        Vec2f center = (wt_box.lu + wt_box.rd) / 2.;
        double w = wt_box.lu.y() - wt_box.ld.y();
        enum Type {
            Arc,
            Corner,
            ArcStart,
            ArcEnd
        };

        // First generate vector of annotated point which form the boundary.
        std::vector<std::pair<Vec2f, Type>> pts = {{wt_box.ru, Corner}};        
        if (double alpha_start = std::asin((0.5*w)/r); ! std::isnan(alpha_start) && r > 0.5*w+0.01) {
            for (double alpha = alpha_start; alpha < M_PI-alpha_start+0.001; alpha+=(M_PI-2*alpha_start) / 40.)
                pts.emplace_back(Vec2f(center.x() + r*std::cos(alpha)/support_scale, center.y() + r*std::sin(alpha)), alpha == alpha_start ? ArcStart : Arc);
            pts.back().second = ArcEnd;
        }        
        pts.emplace_back(wt_box.lu, Corner);
        pts.emplace_back(wt_box.ld, Corner);
        for (int i=int(pts.size())-3; i>0; --i)
            pts.emplace_back(Vec2f(pts[i].first.x(), 2*center.y()-pts[i].first.y()), i == int(pts.size())-3 ? ArcStart : i == 1 ? ArcEnd : Arc);
        pts.emplace_back(wt_box.rd, Corner);

        // Create a Polygon from the points.
        Polygon poly;
        for (const auto& [pt, tag] : pts)
            poly.points.push_back(Point::new_scale(pt));

        // Prepare polygons to be filled by infill.
        Polylines polylines;
        if (infill_cone && m_wipe_tower_width > 2*spacing && m_wipe_tower_depth > 2*spacing) {
            ExPolygons infill_areas;
            ExPolygon wt_contour(poly);
            Polygon wt_rectangle(Points{Point::new_scale(wt_box.ld), Point::new_scale(wt_box.rd), Point::new_scale(wt_box.ru), Point::new_scale(wt_box.lu)});
            wt_rectangle = offset(wt_rectangle, scale_(-spacing/2.)).front();
            wt_contour = offset_ex(wt_contour, scale_(-spacing/2.)).front();
            infill_areas = diff_ex(wt_contour, wt_rectangle);
            if (infill_areas.size() == 2) {
                ExPolygon& bottom_expoly = infill_areas.front().contour.points.front().y() < infill_areas.back().contour.points.front().y() ? infill_areas[0] : infill_areas[1];
                std::unique_ptr<Fill> filler(Fill::new_from_type(ipMonotonicLine));
                filler->angle = Geometry::deg2rad(45.f);
                filler->spacing = spacing;
                FillParams params;
                params.density = 1.f;
                Surface surface(stBottom, bottom_expoly);
                filler->bounding_box = get_extents(bottom_expoly);
                polylines = filler->fill_surface(&surface, params);
                if (! polylines.empty()) {
                    if (polylines.front().points.front().x() > polylines.back().points.back().x()) {
                        std::reverse(polylines.begin(), polylines.end());
                        for (Polyline& p : polylines)
                            p.reverse();
                    }
                }
            }
        }

        // Find the closest corner and travel to it.
        int start_i = 0;
        double min_dist = std::numeric_limits<double>::max();
        for (int i=0; i<int(pts.size()); ++i) {
            if (pts[i].second == Corner) {
                double dist = (pts[i].first - Vec2f(writer.x(), writer.y())).squaredNorm();
                if (dist < min_dist) {
                    min_dist = dist;
                    start_i = i;
                }
            }
        }
        writer.travel(pts[start_i].first);

        // Now actually extrude the boundary (and possibly infill):
        int i = start_i+1 == int(pts.size()) ? 0 : start_i + 1;
        while (i != start_i) {
            writer.extrude(pts[i].first, feedrate);
            if (pts[i].second == ArcEnd) {
                // Extrude the infill.
                if (! polylines.empty()) {
                    // Extrude the infill and travel back to where we were.
                    bool mirror = ((pts[i].first.y() - center.y()) * (unscale(polylines.front().points.front()).y() - center.y())) < 0.;
                    for (const Polyline& line : polylines) {
                        writer.travel(center - (mirror ? 1.f : -1.f) * (unscale(line.points.front()).cast<float>() - center));
                        for (size_t i=0; i<line.points.size(); ++i)
                            writer.extrude(center - (mirror ? 1.f : -1.f) * (unscale(line.points[i]).cast<float>() - center));
                    }
                    writer.travel(pts[i].first);
                }
            }
            if (++i == int(pts.size()))
                i = 0;
        }
        writer.extrude(pts[start_i].first, feedrate);
        return poly;
    };

    auto chamfer = [this, &writer, spacing, first_layer](const WipeTower::box_coordinates& wt_box, double feedrate)->Polygon{
        WipeTower::box_coordinates _wt_box = wt_box; // align_perimeter(wt_box);
        if (true) {
            writer.rectangle(_wt_box, feedrate);
        }
        
        Polygon poly;
        int loops_num = (m_wipe_tower_brim_width + spacing / 2.f) / spacing;
        const float max_chamfer_width = 3.f;
        if (!first_layer) {
            // stop print chamfer if depth changes
            if (m_layer_info->depth != m_plan.front().depth) {
                loops_num = 0;
            }
            else {
                // limit max chamfer width to 3 mm
                int chamfer_loops_num = (int)(max_chamfer_width / spacing);
                int dist_to_1st = m_layer_info - m_plan.begin() - m_first_layer_idx;
                loops_num = std::min(loops_num, chamfer_loops_num) - dist_to_1st;
            }
        }


        WipeTower::box_coordinates box = _wt_box;
        if (loops_num > 0) {
            for (size_t i = 0; i < loops_num; ++i) {
                box.expand(spacing);
                writer.rectangle(box);
            }
        }

        if(first_layer)
            m_wipe_tower_brim_width_real += loops_num * spacing;
        poly.points.emplace_back(Point::new_scale(box.ru));
        poly.points.emplace_back(Point::new_scale(box.lu));
        poly.points.emplace_back(Point::new_scale(box.ld));
        poly.points.emplace_back(Point::new_scale(box.rd));
        return poly;
    };
    feedrate = first_layer ? m_first_layer_speed * 60.f : m_perimeter_speed * 60.f;

    if(first_layer)
        m_wipe_tower_brim_width_real = 0.0f;
    // outer contour (always)
    bool use_cone = m_prime_tower_enhance_type == PrimeTowerEnhanceType::pteCone;
    Polygon poly;
    if(use_cone){
        bool infill_cone = first_layer && m_wipe_tower_width > 2*spacing && m_wipe_tower_depth > 2*spacing;
        poly = supported_rectangle(wt_box, feedrate, infill_cone);
    }else{
        poly = chamfer(wt_box, feedrate);
    }


    // brim (first layer only)
    if (first_layer) {
        size_t loops_num = (m_wipe_tower_brim_width + spacing/2.f) / spacing;
        
        for (size_t i = 0; i < loops_num; ++ i) {
            poly = offset(poly, scale_(spacing)).front();
            int cp = poly.closest_point_index(Point::new_scale(writer.x(), writer.y()));
            writer.travel(unscale(poly.points[cp]).cast<float>());
            for (int i=cp+1; true; ++i ) {
                if (i==int(poly.points.size()))
                    i = 0;
                writer.extrude(unscale(poly.points[i]).cast<float>());
                if (i == cp)
                    break;
            }
        }

        // Save actual brim width to be later passed to the Print object, which will use it
        // for skirt calculation and pass it to GLCanvas for precise preview box
        m_wipe_tower_brim_width_real += loops_num * spacing;
    }

    // Now prepare future wipe.
    int i = poly.closest_point_index(Point::new_scale(writer.x(), writer.y()));
    writer.add_wipe_point(writer.pos());
    writer.add_wipe_point(unscale(poly.points[i==0 ? int(poly.points.size())-1 : i-1]).cast<float>());

    // Ask our writer about how much material was consumed.
    // Skip this in case the layer is sparse and config option to not print sparse layers is enabled.
    if (! m_no_sparse_layers || toolchanges_on_layer || first_layer) {
        if (m_current_tool < m_used_filament_length.size())
            m_used_filament_length[m_current_tool] += writer.get_and_reset_used_filament_length();
        m_current_height += m_layer_info->height;
    }

    return construct_tcr(writer,false, old_tool, true);
}

// Appends a toolchange into m_plan and calculates neccessary depth of the corresponding box
void WipeTowerCreality::plan_toolchange(float z_par, float layer_height_par, unsigned int old_tool,
                                unsigned int new_tool, float wipe_volume)
{
	assert(m_plan.empty() || m_plan.back().z <= z_par + WT_EPSILON);	// refuses to add a layer below the last one

	if (m_plan.empty() || m_plan.back().z + WT_EPSILON < z_par) // if we moved to a new layer, we'll add it to m_plan first
		m_plan.push_back(WipeTowerInfo(z_par, layer_height_par));

    if (m_first_layer_idx == size_t(-1) && (! m_no_sparse_layers || old_tool != new_tool || m_plan.size() == 1))
        m_first_layer_idx = m_plan.size() - 1;

    if (old_tool == new_tool)	// new layer without toolchanges - we are done
        return;

    float depth = 0.f;
    float width = m_wipe_tower_width - 2 * m_perimeter_width;

    // BBS: if the wipe tower width is too small, the depth will be infinity
    if (width <= EPSILON)
        return;

    float length_to_extrude = volume_to_length(wipe_volume, m_perimeter_width, layer_height_par);
    depth += std::ceil(length_to_extrude / width) * m_perimeter_width;

	m_plan.back().tool_changes.push_back(WipeTowerInfo::ToolChange(old_tool, new_tool, depth, 0.0f, 0.0f, wipe_volume));    
}



void WipeTowerCreality::plan_tower()
{
	// Calculate m_wipe_tower_depth (maximum depth for all the layers) and propagate depths downwards
	m_wipe_tower_depth = 0.f;
	for (auto& layer : m_plan)
		layer.depth = 0.f;
    m_wipe_tower_height = m_plan.empty() ? 0.f : m_plan.back().z;
    m_current_height = 0.f;
	
    for (int layer_index = int(m_plan.size()) - 1; layer_index >= 0; --layer_index)
	{
		float this_layer_depth = std::max(m_plan[layer_index].depth, m_plan[layer_index].toolchanges_depth());
		m_plan[layer_index].depth = this_layer_depth;
		
		if (this_layer_depth > m_wipe_tower_depth - m_perimeter_width)
			m_wipe_tower_depth = this_layer_depth + m_perimeter_width;

		for (int i = layer_index - 1; i >= 0 ; i--)
		{
			if (m_plan[i].depth - this_layer_depth < 2*m_perimeter_width )
				m_plan[i].depth = this_layer_depth;
		}
	}
}

// Return index of first toolchange that switches to non-soluble extruder
// ot -1 if there is no such toolchange.
int WipeTowerCreality::first_toolchange_to_nonsoluble(
        const std::vector<WipeTowerInfo::ToolChange>& tool_changes) const
{
    for (size_t idx=0; idx<tool_changes.size(); ++idx)
#if 1
        return 0;
#else
        if (! m_filpar[tool_changes[idx].new_tool].is_soluble)
            return idx;
#endif

    return -1;
}

static WipeTower::ToolChangeResult merge_tcr(WipeTower::ToolChangeResult& first,
                                             WipeTower::ToolChangeResult& second)
{
    assert(first.new_tool == second.initial_tool);
    WipeTower::ToolChangeResult out = first;
    if (first.end_pos != second.start_pos)
        out.gcode += "G1 X" + Slic3r::float_to_string_decimal_point(second.start_pos.x(), 3)
                     + " Y" + Slic3r::float_to_string_decimal_point(second.start_pos.y(), 3)
                     + " F7200\n";
    out.gcode += second.gcode;
    out.extrusions.insert(out.extrusions.end(), second.extrusions.begin(), second.extrusions.end());
    out.end_pos = second.end_pos;
    out.wipe_path = second.wipe_path;
    out.initial_tool = first.initial_tool;
    out.new_tool = second.new_tool;
    return out;
}


WipeTower::ToolChangeResult WipeTowerCreality::only_generate_out_wall()
{
    size_t old_tool = m_current_tool;

    WipeTowerWriterCreality writer(m_layer_height, m_perimeter_width, m_gcode_flavor, m_filpar);
    writer.set_extrusion_flow(m_extrusion_flow)
        .set_z(m_z_pos)
        .set_initial_tool(m_current_tool)
        .set_y_shift(m_y_shift - (m_current_shape == SHAPE_REVERSED ? m_layer_info->toolchanges_depth() : 0.f));

    // Slow down on the 1st layer.
    bool first_layer = is_first_layer();
    // BBS: speed up perimeter speed to 90mm/s for non-first layer
    float           feedrate   = first_layer ? std::min(m_first_layer_speed * 60.f, 5400.f) :
                                               std::min(60.0f * m_filpar[m_current_tool].max_e_speed / m_extrusion_flow, 5400.f);
    float           fill_box_y = m_layer_info->toolchanges_depth() + m_perimeter_width;
    WipeTower::box_coordinates fill_box(Vec2f(m_perimeter_width, fill_box_y), m_wipe_tower_width - 2 * m_perimeter_width,
                             m_layer_info->depth - fill_box_y);

    writer.set_initial_position((m_left_to_right ? fill_box.ru : fill_box.lu), // so there is never a diagonal travel
                                m_wipe_tower_width, m_wipe_tower_depth, m_internal_rotation);

    bool toolchanges_on_layer = m_layer_info->toolchanges_depth() > WT_EPSILON;

    // we are in one of the corners, travel to ld along the perimeter:
    // BBS: Delete some unnecessary travel
    // if (writer.x() > fill_box.ld.x() + EPSILON) writer.travel(fill_box.ld.x(), writer.y());
    // if (writer.y() > fill_box.ld.y() + EPSILON) writer.travel(writer.x(), fill_box.ld.y());
    writer.append(";" + GCodeProcessor::reserved_tag(GCodeProcessor::ETags::Wipe_Tower_Start) + "\n");
    // outer perimeter (always):
    // BBS
    WipeTower::box_coordinates wt_box(Vec2f(0.f, (m_current_shape == SHAPE_REVERSED ? m_layer_info->toolchanges_depth() : 0.f)),
                                    m_wipe_tower_width,
                           m_layer_info->depth + m_perimeter_width);
    wt_box = align_perimeter(wt_box);
    writer.rectangle(wt_box, feedrate);

    // Now prepare future wipe. box contains rectangle that was extruded last (ccw).
    Vec2f target = (writer.pos() == wt_box.ld ?
                        wt_box.rd :
                        (writer.pos() == wt_box.rd ? wt_box.ru : (writer.pos() == wt_box.ru ? wt_box.lu : wt_box.ld)));
    writer.add_wipe_point(writer.pos()).add_wipe_point(target);

    writer.append(";" + GCodeProcessor::reserved_tag(GCodeProcessor::ETags::Wipe_Tower_End) + "\n");

    // Ask our writer about how much material was consumed.
    // Skip this in case the layer is sparse and config option to not print sparse layers is enabled.
    if (!m_no_sparse_layers || toolchanges_on_layer)
        if (m_current_tool < m_used_filament_length.size())
            m_used_filament_length[m_current_tool] += writer.get_and_reset_used_filament_length();

    return construct_tcr(writer, false, old_tool, true);
}



// Processes vector m_plan and calls respective functions to generate G-code for the wipe tower
// Resulting ToolChangeResults are appended into vector "result"
void WipeTowerCreality::generate(std::vector<std::vector<WipeTower::ToolChangeResult>> &result)
{
	if (m_plan.empty())
        return;

	plan_tower();
    // for (int i=0;i<5;++i) {
    //     save_on_last_wipe();
    //     plan_tower();
    // }

    m_layer_info = m_plan.begin();
    m_current_height = 0.f;

    // we don't know which extruder to start with - we'll set it according to the first toolchange
    for (const auto& layer : m_plan) {
        if (!layer.tool_changes.empty()) {
            m_current_tool = layer.tool_changes.front().old_tool;
            break;
        }
    }

    for (auto& used : m_used_filament_length) // reset used filament stats
        used = 0.f;

    m_old_temperature = -1; // reset last temperature written in the gcode

    std::vector<WipeTower::ToolChangeResult> layer_result;
	for (const WipeTowerCreality::WipeTowerInfo& layer : m_plan)
	{
        set_layer(layer.z, layer.height, 0, false/*layer.z == m_plan.front().z*/, layer.z == m_plan.back().z);
        m_internal_rotation += 180.f;

        if (m_layer_info->depth < m_wipe_tower_depth - m_perimeter_width)
			m_y_shift = (m_wipe_tower_depth-m_layer_info->depth-m_perimeter_width)/2.f;

        int idx = first_toolchange_to_nonsoluble(layer.tool_changes);
        WipeTower::ToolChangeResult finish_layer_tcr;
        WipeTower::ToolChangeResult timelapse_wall;
        if (idx == -1) {
            // if there is no toolchange switching to non-soluble, finish layer
            // will be called at the very beginning. That's the last possibility
            // where a nonsoluble tool can be.
            //finish_layer_tcr = finish_layer();
            if (m_enable_timelapse_print) {
                timelapse_wall = only_generate_out_wall();
            }
            finish_layer_tcr = finish_layer(m_enable_timelapse_print ? false : true, layer.extruder_fill);
        }

        for (int i=0; i<int(layer.tool_changes.size()); ++i) {
            //layer_result.emplace_back(tool_change(layer.tool_changes[i].new_tool));
            //if (i == idx) // finish_layer will be called after this toolchange
            //    finish_layer_tcr = finish_layer();

              if (i == 0 && m_enable_timelapse_print) {
                timelapse_wall = only_generate_out_wall();
              }

            if (i == idx) {
                layer_result.emplace_back(tool_change(layer.tool_changes[i].new_tool, m_enable_timelapse_print ? false : true));
                // finish_layer will be called after this toolchange
                finish_layer_tcr = finish_layer(false, layer.extruder_fill);
            } else {
                if (idx == -1 && i == 0) {
                    layer_result.emplace_back(tool_change(layer.tool_changes[i].new_tool, false, true));
                } else {
                    layer_result.emplace_back(tool_change(layer.tool_changes[i].new_tool));
                }
            }
        }

        if (layer_result.empty()) {
            // there is nothing to merge finish_layer with
            layer_result.emplace_back(std::move(finish_layer_tcr));
        }
        else {
            if (idx == -1) {
                layer_result[0] = merge_tcr(finish_layer_tcr, layer_result[0]);
            }
            else
                layer_result[idx] = merge_tcr(layer_result[idx], finish_layer_tcr);
        }

		result.emplace_back(std::move(layer_result));
	}
}



std::vector<std::pair<float, float>> WipeTowerCreality::get_z_and_depth_pairs() const
{
    std::vector<std::pair<float, float>> out = {{0.f, m_wipe_tower_depth}};
    for (const WipeTowerInfo& wti : m_plan) {
        assert(wti.depth < wti.depth + WT_EPSILON);
        if (wti.depth < out.back().second - WT_EPSILON)
            out.emplace_back(wti.z, wti.depth);
    }
    if (out.back().first < m_wipe_tower_height - WT_EPSILON)
        out.emplace_back(m_wipe_tower_height, 0.f);
    return out;
}

} // namespace Slic3r
