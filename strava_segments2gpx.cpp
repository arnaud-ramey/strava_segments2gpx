// C
#include <curl/curl.h>
#include <dirent.h> // opendir()
#include <math.h> // sqrt()
#include <unistd.h> // acces()
// C++ includes
#include <algorithm>
#include <fstream>
#include <map>
#include <sstream>
#include <vector>

inline int all_files_in_dir(const std::string & folder,
                            std::vector<std::string> & ans,
                            const std::string pattern = "") {
  ans.clear();
  std::string folder_and_slash = folder + std::string("/");
  DIR *dir = opendir (folder.c_str());
  if (dir == NULL) {
    printf("all_files_in_dir:could not open directory:'%s'\n", folder.c_str());
    return -1;
  }
  struct dirent *ent;
  while ((ent = readdir (dir)) != NULL) {
    std::string filename(ent->d_name);
    if (pattern.size() && filename.find(pattern) == std::string::npos)
      continue;
    ans.push_back(folder_and_slash + filename);
  }
  closedir (dir);
  std::sort(ans.begin(), ans.end());
  return ans.size();
} // end all_files_in_dir()

////////////////////////////////////////////////////////////////////////////////

inline bool file_exists(const std::string & filename) {
  return access( filename.c_str(), F_OK ) != -1;
}

////////////////////////////////////////////////////////////////////////////////

// https://stackoverflow.com/questions/236129/the-most-elegant-way-to-iterate-the-words-of-a-string/236803#236803
template<typename Out>
void split(const std::string &s, char delim, Out result) {
  std::stringstream ss(s);
  std::string item;
  while (std::getline(ss, item, delim)) {
    *(result++) = item;
  }
}

std::vector<std::string> split(const std::string &s, char delim) {
  std::vector<std::string> elems;
  split(s, delim, std::back_inserter(elems));
  return elems;
}

////////////////////////////////////////////////////////////////////////////////

inline int find_and_replace(std::string& stringToReplace,
                            const std::string & pattern,
                            const std::string & patternReplacement) {
  size_t j = 0;
  int nb_found_times = 0;
  for (; (j = stringToReplace.find(pattern, j)) != std::string::npos;) {
    //cout << "found " << pattern << endl;
    stringToReplace.replace(j, pattern.length(), patternReplacement);
    j += patternReplacement.length();
    ++ nb_found_times;
  }
  return nb_found_times;
}

////////////////////////////////////////////////////////////////////////////////

//! This is the writer call back function used by curl
int curl_writer(char *data, size_t size, size_t nmemb, std::string *buffer) {
  // What we will return
  int result = 0;

  // Is there anything in the buffer?
  if (buffer != NULL) {
    // Append the data to the buffer
    buffer->append(data, size * nmemb);

    // How much did we write?
    result = size * nmemb;
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////

/*! get the content of a distant url
 * \param url for instance http://www.google.es
 * \param ans where we save the answer
 * \return true if success, false otherwise
 */
// A big part of this code was obtained by calling :
//  curl -G https://www.strava.com/api/v3/segments/explore -H "Authorization: Bearer XXX" -d bounds=37.821362,-122.505373,37.842038,-122.465977 --libcurl /tmp/foo.c
bool retrieve_url(const std::string & url,
                  const std::string & token,
                  std::string & ans) {
  //printf("retrieve_url('%s')", extract(url).c_str());
  ans = "";
  // Create our curl handle
  CURL *hnd = curl_easy_init();
  if (!hnd) {
    printf("Could not initialize curl!");
    return false;
  }

  // Now set up all of the curl options
  // authentification
  // https://stackoverflow.com/posts/30426047/revisions
  // https://curl.haxx.se/libcurl/c/CURLOPT_HTTPHEADER.html
  struct curl_slist *list = NULL;
  std::string auth = "Authorization: Bearer " + token;
  list = curl_slist_append(list, auth.c_str());
  curl_easy_setopt(hnd, CURLOPT_HTTPHEADER, list);
  // Write any errors in here
  static char errorBuffer[CURL_ERROR_SIZE];
  curl_easy_setopt(hnd, CURLOPT_ERRORBUFFER, errorBuffer);
  // now the other options
  curl_easy_setopt(hnd, CURLOPT_URL, url.c_str());
  curl_easy_setopt(hnd, CURLOPT_MAXREDIRS, 50L);
  curl_easy_setopt(hnd, CURLOPT_NOPROGRESS, 1L);
  curl_easy_setopt(hnd, CURLOPT_TCP_KEEPALIVE, 1L);
  curl_easy_setopt(hnd, CURLOPT_USERAGENT, "curl/7.47.0");
  curl_easy_setopt(hnd, CURLOPT_WRITEDATA, &ans);
  curl_easy_setopt(hnd, CURLOPT_WRITEFUNCTION, curl_writer);

  // Attempt to retrieve the remote page
  CURLcode result = curl_easy_perform(hnd);

  // Always cleanup
  curl_easy_cleanup(hnd);
  curl_slist_free_all(list); /* free the list again */
  // Did we succeed?
  if (result != CURLE_OK) {
    printf("Error: [%i] : '%s'\n", result, errorBuffer);
    return false;
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////

//! key: id, example: 6777764
//! value: name, example: "Allogny -> Les Rousseaux Climb"
typedef std::map<int, std::string> SegmentList;

////////////////////////////////////////////////////////////////////////////////

bool parse_response(const std::string & response,
                    SegmentList & out) {
  std::string tag_id = "\"id\":", tag_name = "\"name\":\"";
  std::string::size_type pos = 0;
  while (true) {
    // find id
    std::string::size_type id_begin = response.find(tag_id, pos);
    std::string::size_type id_end = response.find(',', id_begin);
    if (id_begin == std::string::npos || id_end == std::string::npos)
      return true;
    std::string id_str = response.substr(id_begin + tag_id.size(),
                                         id_end-id_begin - tag_id.size());
    unsigned int id = atoi(id_str.c_str());
    // find name
    std::string::size_type name_begin = response.find(tag_name, id_end+1);
    std::string::size_type name_end = response.find(',', name_begin);
    if (name_begin == std::string::npos || name_end == std::string::npos)
      return true;
    std::string name = response.substr(name_begin + tag_name.size(),
                                       name_end-name_begin - tag_name.size() - 1);
    printf("id:%i,\t name:'%s'\n", id, name.c_str());
    out.insert(std::make_pair(id, name));
    pos = name_end+1;
  }
}

////////////////////////////////////////////////////////////////////////////////

// https://strava.github.io/api/v3/segments/
//  curl -G https://www.strava.com/api/v3/segments/explore
//       -H "Authorization: Bearer 83ebeabdec09f6670863766f792ead24d61fe3f9"
//       -d bounds=37.821362,-122.505373,37.842038,-122.465977
// -H = --header
// -d = --data sw.lat,sw.lng,ne.lat,ne.lng = south,west,north,east

bool retrieve10around(const std::string & token,
                      const double & south, const double & west,
                      const double & north, const double & east,
                      SegmentList & out) {
  std::ostringstream url;
  url << "https://www.strava.com/api/v3/segments/explore?bounds="
      << south << "," << west << "," << north << "," << east;
  //curl_easy_setopt(hnd, CURLOPT_URL, "https://www.strava.com/api/v3/segments/explore?bounds=37.821362,-122.505373,37.842038,-122.465977");
  std::string out_str;
  if (!retrieve_url(url.str(), token, out_str))
    return false;
  //out_str = "{\"segments\":[{\"id\":229781,\"resource_state\":2,\"name\":\"Hawk Hill\",\"climb_category\":1,\"climb_category_desc\":\"4\",\"avg_grade\":5.7,\"start_latlng\":[37.8331119,-122.4834356],\"end_latlng\":[37.8280722,-122.4981393],\"elev_difference\":152.8,\"distance\":2684.8,\"points\":\"}g|eFnpqjVl@En@Md@HbAd@d@^h@Xx@VbARjBDh@OPQf@w@d@k@XKXDFPH\\EbGT`AV`@v@|@NTNb@?XOb@cAxAWLuE@eAFMBoAv@eBt@q@b@}@tAeAt@i@dAC`AFZj@dB?~@[h@MbAVn@b@b@\\d@Eh@Qb@_@d@eB|@c@h@WfBK|AMpA?VF\\\\t@f@t@h@j@|@b@hCb@b@XTd@Bl@GtA?jAL`ALp@Tr@RXd@Rx@Pn@^Zh@Tx@Zf@`@FTCzDy@f@Yx@m@n@Op@VJr@\",\"starred\":false},{\"id\":632535,\"resource_state\":2,\"name\":\"Hawk Hill Upper Conzelman to Summit\",\"climb_category\":0,\"climb_category_desc\":\"NC\",\"avg_grade\":8.1,\"start_latlng\":[37.8334451,-122.4941994],\"end_latlng\":[37.8281297,-122.4980005],\"elev_difference\":67.3,\"distance\":829.8,\"points\":\"_j|eFvssjV\\l@`@j@h@b@vD~@^b@Nt@GdBDxAL|@VhAXf@d@PdATTNb@f@^dA\\^b@JV?lCc@p@SbAu@h@Qn@?RTDH\",\"starred\":false},{\"id\":2371095,\"resource_state\":2,\"name\":\"McCullough Rd. (Headlands)\",\"climb_category\":1,\"climb_category_desc\":\"4\",\"avg_grade\":8.5,\"start_latlng\":[37.836073,-122.5023015],\"end_latlng\":[37.8338899,-122.4941542],\"elev_difference\":116.8,\"distance\":1368.9,\"points\":\"mz|eFlfujV|@iB`@cANs@HyDAOOs@Uc@e@]u@Uc@[Kk@Dk@n@aBZeAl@_BHi@C[S_@]Mq@Fy@Nw@D[QQ]Km@Uu@c@_@i@Y[a@Ai@Pa@JE^AXTv@xAj@b@f@Jh@C`@Of@a@hByCjBcBl@q@n@a@z@Qr@U\",\"starred\":false},{\"id\":638282,\"resource_state\":2,\"name\":\"Hawk Hill Final Sprint\",\"climb_category\":0,\"climb_category_desc\":\"NC\",\"avg_grade\":5.6,\"start_latlng\":[37.83061,-122.49829],\"end_latlng\":[37.82825,-122.49805],\"elev_difference\":23.3,\"distance\":287.9,\"points\":\"ix{eFhmtjVHPJJh@XPAHB`@?\\OjAOPEFEd@K\\Mv@a@r@SXEHJBJ\",\"starred\":false},{\"id\":11345963,\"resource_state\":2,\"name\":\"Hawk Top\",\"climb_category\":0,\"climb_category_desc\":\"NC\",\"avg_grade\":5.2,\"start_latlng\":[37.832571,-122.491626],\"end_latlng\":[37.83362,-122.493605],\"elev_difference\":12.8,\"distance\":237.7,\"points\":\"qd|eFtcsjVSZeCdAOLINKd@@n@AXQzC\",\"starred\":false},{\"id\":4957941,\"resource_state\":2,\"name\":\"Alexander Uphill Smash\",\"climb_category\":0,\"climb_category_desc\":\"NC\",\"avg_grade\":-0.3,\"start_latlng\":[37.838181,-122.482944],\"end_latlng\":[37.840009,-122.478403],\"elev_difference\":10.4,\"distance\":530.9,\"points\":\"sg}eFlmqjV{@D]AYC[GYMq@g@k@o@e@w@O_@WcAMeAAc@@iAFgALoFE_AM{@IY\",\"starred\":false},{\"id\":635252,\"resource_state\":2,\"name\":\"Hawk Hill False Flats\",\"climb_category\":0,\"climb_category_desc\":\"NC\",\"avg_grade\":3.5,\"start_latlng\":[37.8297126665711,-122.483360655606],\"end_latlng\":[37.8337470442057,-122.494189823046],\"elev_difference\":50.0,\"distance\":1413.0,\"points\":\"ur{eF`pqjVXLDFDT?zBKvA?JHb@L`@JPjApATXDV@VGj@MT_@j@Ur@UT[F[CsABs@DaA?_@Bo@R_Ar@sAf@cAr@MR_@r@aAv@i@|@MXC^@NBNj@~@Pj@D^E\\q@fAA^BNLZ`A~@JXB^ANIXYZYRcBx@MNQZMh@MpBSfBa@rABVFD\",\"starred\":false},{\"id\":7805335,\"resource_state\":2,\"name\":\"Upper McCullough\",\"climb_category\":0,\"climb_category_desc\":\"NC\",\"avg_grade\":8.8,\"start_latlng\":[37.837605,-122.49609],\"end_latlng\":[37.833928,-122.494164],\"elev_difference\":49.6,\"distance\":561.4,\"points\":\"_d}eFp_tjVIKGOCQ?[DOPQRAZJbAbBVVLHZJ\\BVA\\K`@WV[lB_DzCuCb@WtA[TG\",\"starred\":false},{\"id\":960626,\"resource_state\":2,\"name\":\"Ft. Baker to GGB NW Parking Lot\",\"climb_category\":0,\"climb_category_desc\":\"NC\",\"avg_grade\":7.9,\"start_latlng\":[37.8312661,-122.47889],\"end_latlng\":[37.8322157,-122.4822012],\"elev_difference\":63.9,\"distance\":808.7,\"points\":\"k|{eF`tpjV^v@f@t@|@l@\\`@NVLr@OPe@HWT_@j@YH_BG]LIPCv@Xd@`@Td@Ln@@h@Ml@Wf@AFBT^Cd@Yb@c@PeFbAm@AeAO\",\"starred\":false},{\"id\":4021176,\"resource_state\":2,\"name\":\"horseshoe bay\",\"climb_category\":0,\"climb_category_desc\":\"NC\",\"avg_grade\":-4.0,\"start_latlng\":[37.8392,-122.475016],\"end_latlng\":[37.834472,-122.478239],\"elev_difference\":36.6,\"distance\":923.9,\"points\":\"_n}eFz{ojV|@o@hBoB|@a@ZWf@i@d@_A`@g@LI|@Q`ANz@p@NNn@dANbAGfB?xAFjFAxCBTTd@n@`@|EhC\",\"starred\":false}]}";
  return parse_response(out_str, out);
}

////////////////////////////////////////////////////////////////////////////////

bool segments_list2file(const std::string & token,
                        const double & clat, const double & clon,
                        const double & radius_km,
                        const int nlat,
                        const std::string & segmentsfile) {
  printf("segments_list2file(lat=%g°, lon=%g°, %g km, nlat=%i)\n",
         clat, clon, radius_km, nlat);
  // convert radius_km to degree
  // https://en.wikipedia.org/wiki/Decimal_degrees
  // 1 decimal degree =
  //   111 km if lat=0°
  //   102 km if lat=23°
  //   79 km  if lat=45°
  //   43 km  if lat=67°
  // Polynomial regression according to LibreOffice:
  // km = -0.0139978771 * lat^2 - 0.0786772528 * lat + 111.0521193223
  const double a = -0.0139978771, b = -0.0786772528, c = 111.0521193223;
  double DEG2KM_LON = a * clat * clat + b * clat + c; // km/deg
  double KM2DEG_LON = 1. / DEG2KM_LON, KM2DEG_LAT = 1. / 111;
  double radius_deg_lon = radius_km * KM2DEG_LON, radius_deg_lat = radius_km * KM2DEG_LAT;
  printf("radius:%g km -> as_deg: lat:%g°, lon:%g°\n",
         radius_km, radius_deg_lat, radius_deg_lon);
  //
  int halfnlat = (int) (nlat/2); // nlat must be odd!
  double winw = radius_deg_lon / nlat, winh = radius_deg_lat / nlat;
  int nsteps = nlat * nlat, counter = 0;
  SegmentList list;

  for (int ilat = -halfnlat; ilat <= halfnlat; ++ilat) {
    double lat = clat + ilat * winh;
    double south = lat - .6 * winh, north = lat + .6 * winh; // should be .5 * winh but that way we have overlap

    for (int ilon = -halfnlat; ilon <= halfnlat; ++ilon) {
      double lon = clon + ilon * winw;
      double west = lon - .6 * winw, east = lon + .6 * winw; // idem
      printf("%i/%i = %i %%,\t cell (%i, %i)\t = coordinates (%g, %g, %g, %g)\n",
             counter, nsteps, 100 * counter / nsteps,
             ilat, ilon, south, west, north, east);
      if (!retrieve10around(token, south, west, north, east, list))
        return false;
      ++counter;
    } // end for ilon
  } // end for ilat

  // write to file
  std::ofstream myfile;
  myfile.open (segmentsfile.c_str());
  if (!myfile.is_open()) {
    printf("Could not open '%s' for writing\n", segmentsfile.c_str());
    return false;
  }
  printf("Got %li segments\n", list.size());
  for(SegmentList::const_iterator it = list.begin(); it != list.end(); ++ it) {
    //printf("%i\t = '%s'\n", it->first, it->second.c_str());
    myfile << it->first << "," << it->second.c_str() << std::endl;
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////

bool file2gpx(const std::string & segmentsfile,
              const std::string & token,
              const std::string & outdir = "/tmp/") {
  printf("\nfile2gpx('%s' -> %s)\n", segmentsfile.c_str(), outdir.c_str());
  std::string line;
  std::ifstream myfile (segmentsfile.c_str());
  if (!myfile.is_open()) {
    printf("Could not open '%s' for reading\n", segmentsfile.c_str());
    return false;
  }
  unsigned int counter = 0;
  std::string gopath = getenv("GOPATH");
  if (gopath.empty()) {
    printf("Could not find GOPATH, did you install Go? Check doc.\n");
    return false;
  }
  std::string goexe_path = gopath + "/bin/strava-segment-to-gpx";
  if (!file_exists(goexe_path)) {
    printf("Could not find 'strava-segment-to-gpx' Go exe, did you install deps? Check doc.\n");
    return false;
  }

  while ( std::getline (myfile,line) ) {
    std::string::size_type comma = line.find(',');
    unsigned int id = atoi(line.substr(0, comma).c_str());
    printf("File %i: segment id=%i\n", counter++, id);
    std::ostringstream cmd, outfile;
    outfile << outdir << '/' << id << ".gpx";
    if (file_exists(outfile.str())) {
      printf("GPX file '%s' already exists!\n", outfile.str().c_str());
      continue;
    }
    // /home/arnaud/src/go/bin/strava-segment-to-gpx -token XXX -id $ID > $OUT
    cmd << goexe_path << "  -token " << token << " -id " << id << " > " << outfile.str();
    //printf("cmd:'%s'\n", cmd.str().c_str());
    if (system(cmd.str().c_str()))
      return false;
  }
  myfile.close();
  printf("Wrote %i GPX files to '%s'\n", counter, outdir.c_str());
  return true;
} // end file2gpx

////////////////////////////////////////////////////////////////////////////////

struct Pt2 {
  double lat, lon; // lat = y
};
struct Segment : std::vector<Pt2>  {
  std::string name;
  unsigned int id;
};

void offset(Segment & s, const double & offset_dd) {
  if (s.empty())
    return;
  unsigned int npts = s.size();
  double dx = 0, dy = 0, norm_inv = 1;
  for (unsigned int i = 0; i < npts-1; ++i) {
    dx = s[i+1].lon - s[i].lon;
    dy = s[i+1].lat - s[i].lat;
    norm_inv = 1. / sqrt(dx * dx + dy * dy);
    s[i].lon += + offset_dd * dy * norm_inv;
    s[i].lat += - offset_dd * dx * norm_inv;
  }
  // also apply it on the last point
  s.back().lon += + offset_dd * dy * norm_inv;
  s.back().lat += - offset_dd * dx * norm_inv;
}

////////////////////////////////////////////////////////////////////////////////

bool gpx2final(const std::string & outdir,
               const std::string & outfile) {
  printf("\ngpx2final(%s/*.gpx -> '%s')\n", outdir.c_str(), outfile.c_str());
  // write to file
  std::ofstream outstr;
  outstr.open (outfile.c_str());
  if (!outstr.is_open()) {
    printf("Could not open '%s' for writing\n", outfile.c_str());
    return false;
  }

  // read each file
  std::vector<std::string> files;
  std::vector<Segment> segments;
  all_files_in_dir(outdir, files, ".gpx");
  for (unsigned int i = 0; i < files.size(); ++i) {
    std::ifstream myfile (files[i].c_str());
    if (!myfile.is_open()) {
      printf("Could not open '%s' for reading\n", files[i].c_str());
      continue;
    }
    segments.push_back(Segment());
    std::string line;
    while ( std::getline (myfile,line) ) {
      if (line.find("<name>") != std::string::npos) {
        std::string name = line.substr(8, line.size() - 15); // get rid of <name> and </name<
        // get rid of HTML entities
        find_and_replace(name, "&gt;", ">");
        find_and_replace(name, "&#34;", "'");
        find_and_replace(name, "&#39;", "'");
        find_and_replace(name, "\\", "");
        segments.back().name = name;
        continue;
      }
      if (line.find("<link") != std::string::npos) {
        std::string ids = line;
        find_and_replace(ids, "  <link href=\"https://www.strava.com/segments/", "");
        find_and_replace(ids, "\"></link>", "");
        segments.back().id = atoi(ids.c_str());
        continue;
      }
      if (line.find("<trkpt") == std::string::npos)
        continue;
      std::vector<std::string> words = split(line, '"');
      if(words.size() != 5) {
        printf("Line '%s' is not well formatted\n", line.c_str());
        continue;
      }
      Pt2 pt;
      pt.lon = atof(words[1].c_str());
      pt.lat = atof(words[3].c_str());
      if (words[2].find("lon") != std::string::npos)
        std::swap(pt.lat, pt.lon);
      segments.back().push_back(pt);
    } // end while (getline)
    if (segments.back().empty()) // erase empty
      segments.pop_back();
  } // end for i
  printf("Read %li segments from %li GPX files in folder '%s'\n",
         segments.size(), files.size(), outdir.c_str());

  // offset segments
  for (unsigned int is = 0; is < segments.size(); ++is)
    offset(segments[is], 1E-3);

  // write to file
  outstr << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << std::endl;
  outstr << "<gpx version=\"1.1\" creator=\"strava_segments2gpx\" "
            "xmlns=\"http://www.topografix.com/GPX/1/1\">" << std::endl;
  // write first point as waypoint
  for (unsigned int is = 0; is < segments.size(); ++is) {
    outstr << "  <!-- \"" << segments[is].name
           << "\" link: https://www.strava.com/segments/" << segments[is].id << " -->" << std::endl;
    Pt2 begin = segments[is].front();
    outstr << "  <wpt lat=\"" << begin.lat << "\" lon=\"" << begin.lon << "\" >" << std::endl;
    outstr << "    <name>" << segments[is].name << "</name>" << std::endl;
    outstr << "    <sym>Bike Trail</sym>" << std::endl;
    outstr << "  </wpt>" << std::endl;
    Pt2 end = segments[is].back();
    outstr << "  <wpt lat=\"" << end.lat << "\" lon=\"" << end.lon << "\" >" << std::endl;
    outstr << "    <sym>Toll Booth</sym>" << std::endl;
    outstr << "  </wpt>" << std::endl;
  } // end for is
  // write all points
  outstr << "  <trk>" << std::endl;
  outstr << "    <name>Strava segments</name>" << std::endl;
  outstr << "    <type>Ride</type>" << std::endl;
  for (unsigned int is = 0; is < segments.size(); ++is) {
    outstr << "    <!-- \"" << segments[is].name
           << "\" link: https://www.strava.com/segments/" << segments[is].id << " -->" << std::endl;
    outstr << "    <trkseg>" << std::endl;
    for (unsigned int ip = 0; ip < segments[is].size(); ++ip) {
      Pt2 p = segments[is][ip];
      outstr << "      <trkpt lat=\"" << p.lat << "\" lon=\"" << p.lon << "\" />" << std::endl;
    } // end for ip
    outstr << "    </trkseg>" << std::endl;
  } // end for is
  outstr << "  </trk>" << std::endl;
  outstr << "</gpx>" << std::endl;
  printf("Wrote file '%s'\n", outfile.c_str());
  return true;
} // end file2gpx

////////////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[]) {
  if (argc < 5) {
    printf("Synopsis: %s TOKEN LAT LON RADIUS\n", argv[0]);
    printf("Where:\n");
    printf("  TOKEN:  Strava developer token\n");
    printf("  LAT:    latitude in decimal degrees of the center of the search area (for instance your city)\n");
    printf("  LON:    longitude in decimal degrees of the center of the search area\n");
    printf("  RADIUS: radius in kilometers of the search area\n");
    printf("  NLAT:   number of cells in latitude or longitude (so nb of steps=NLAT*NLAT). Default: 3\n");
    printf("  GPXDIR: folder where to store the retrieved segments GPX. Default: '/tmp'\n");
    printf("  OUTFILE:filename of the generated GPX file. Default: 'strava_segments2gpx.gpx'\n");
    printf("\n");
    printf("Example: %s mytoken123  47.0810  2.3988  75\n", argv[0]);
    return -1;
  }
  std::string token = argv[1];
  double clat = atof(argv[2]);
  double clon = atof(argv[3]);
  double radius = atof(argv[4]);
  int nlat =(argc >= 6 ? atof(argv[5]) : 3);
  std::string gpxdir =(argc >= 7 ? argv[6] : "/tmp");
  std::string segmentsfile = "/tmp/segments.csv";
  std::string outfile = "strava_segments2gpx.gpx";

  bool ok = segments_list2file(token, clat, clon, radius, nlat, segmentsfile)
      && file2gpx(segmentsfile, token, gpxdir)
      && gpx2final(gpxdir, outfile);
  return (ok ? 0 : -1);
}
