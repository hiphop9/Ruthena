// Copyright (c) rAthena Dev Teams - Licensed under GNU GPL
// For more information, see LICENCE in the main folder

#include "database.hpp"

#include <iostream>
#include <sstream>

#include "malloc.hpp"
#include "showmsg.hpp"
#include "utilities.hpp"
#include <config/custom.hpp>

using namespace rathena;

bool YamlDatabase::nodeExists( const ryml::NodeRef& node, const std::string& name ){
	return (node.num_children() > 0 && node.has_child(c4::to_csubstr(name)));
}

bool YamlDatabase::nodesExist( const ryml::NodeRef& node, std::initializer_list<const std::string> names ){
	bool missing = false;

	for( const std::string& name : names ){
		if( !this->nodeExists( node, name ) ){
			if(strcmpi(MessageLang,"KOR") == 0)
				ShowError( "필수 노드 \"%s\"이(가) 없습니다.\n", name.c_str() );
			else
				ShowError( "Missing mandatory node \"%s\".\n", name.c_str() );
			missing = true;
		}
	}

	if( missing ){
		if(strcmpi(MessageLang,"KOR") == 0)
			this->invalidWarning( node, "최소 한개 이상의 필수 노드가 없습니다.\n" );
		else
			this->invalidWarning( node, "At least one mandatory node did not exist.\n" );
		return false;
	}

	return true;
}

bool YamlDatabase::verifyCompatibility( const ryml::Tree& tree ){
	if( !this->nodeExists( tree.rootref(), "Header" ) ){
		if(strcmpi(MessageLang,"KOR") == 0)
			ShowError( "\"Header\" 데이터베이스를 찾을 수 없습니다.\n" );
		else
			ShowError( "No database \"Header\" was found.\n" );
		return false;
	}

	const ryml::NodeRef& headerNode = tree["Header"];

	std::string tmpType;

	if( !this->asString( headerNode, "Type", tmpType ) ){
		return false;
	}

	if( this->type != tmpType ){
		if(strcmpi(MessageLang,"KOR") == 0)
			ShowError( "데이터베이스 타입 불일치: %s != %s.\n", this->type.c_str(), tmpType.c_str() );
		else
			ShowError( "Database type mismatch: %s != %s.\n", this->type.c_str(), tmpType.c_str() );
		return false;
	}

	uint16 tmpVersion;

	if( !this->asUInt16( headerNode, "Version", tmpVersion ) ){
		return false;
	}

	if( tmpVersion != this->version ){
		if( tmpVersion > this->version ){
			if(strcmpi(MessageLang,"KOR") == 0)
				ShowError( "데이터베이스 버전 %hu은(는) 지원하지 않습니다. 최대 버전: %hu\n", tmpVersion, this->version );
			else
				ShowError( "Database version %hu is not supported. Maximum version is: %hu\n", tmpVersion, this->version );
			return false;
		}else if( tmpVersion >= this->minimumVersion ){
			if(strcmpi(MessageLang,"KOR") == 0) {
				ShowWarning( "데이터베이스 버전 %hu은(는) 오래되었으므로 업데이트를 해야 합니다. 현재 버전: %hu\n", tmpVersion, this->version );
				ShowWarning( "'" CL_WHITE "%s" CL_RESET "' 파일에서 %s 데이터베이스 %s와(과) 호환성이 떨어집니다.\n", this->currentFile.c_str(), this->type.c_str() );
			} else {
				ShowWarning( "Database version %hu is outdated and should be updated. Current version is: %hu\n", tmpVersion, this->version );
				ShowWarning( "Reduced compatibility with %s database file from '" CL_WHITE "%s" CL_RESET "'.\n", this->type.c_str(), this->currentFile.c_str() );
			}
		}else{
			if(strcmpi(MessageLang,"KOR") == 0)
				ShowError( "데이터베이스 버전 %hu은(는) 지원하지 않습니다. 최소 버전: %hu\n", tmpVersion, this->minimumVersion );
			else
				ShowError( "Database version %hu is not supported anymore. Minimum version is: %hu\n", tmpVersion, this->minimumVersion );
			return false;
		}
	}

	return true;
}

bool YamlDatabase::load(){
	bool ret = this->load( this->getDefaultLocation() );

	this->loadingFinished();

	return ret;
}

bool YamlDatabase::reload(){
	this->clear();

	return this->load();
}

bool YamlDatabase::load(const std::string& path) {
	if(strcmpi(MessageLang,"KOR") == 0)
		ShowStatus("'" CL_WHITE "%s" CL_RESET "'을(를) 불러오는 중..." CL_CLL "\r", path.c_str());
	else
		ShowStatus("Loading '" CL_WHITE "%s" CL_RESET "'..." CL_CLL "\r", path.c_str());
	FILE* f = fopen(path.c_str(), "r");
	if (f == nullptr) {
		if(strcmpi(MessageLang,"KOR") == 0)
			ShowError("'" CL_WHITE "%s" CL_RESET "'에서 데이터베이스 파일 %s을(를) 열지 못했습니다.\n", this->type.c_str(), path.c_str());
		else
			ShowError("Failed to open %s database file from '" CL_WHITE "%s" CL_RESET "'.\n", this->type.c_str(), path.c_str());
		return false;
	}
	fseek(f, 0, SEEK_END);
	size_t size = ftell(f);
	char* buf = (char *)aMalloc(size+1);
	rewind(f);
	size_t real_size = fread(buf, sizeof(char), size, f);
	// Zero terminate
	buf[real_size] = '\0';
	fclose(f);

	parser = {};
	ryml::Tree tree;

	try{
		tree = parser.parse_in_arena(c4::to_csubstr(path), c4::to_csubstr(buf));
	}catch( const std::runtime_error& e ){
		if(strcmpi(MessageLang,"KOR") == 0) {
			ShowError( "데이터베이스 파일 %s을(를) '" CL_WHITE "%s" CL_RESET "'에서 불러오지 못했습니다.\n", this->type.c_str(), path.c_str() );
			ShowError( "파일에 구문 오류가 있을 수 있습니다.\n" );
			ShowError( "에러 메시지: %s\n", e.what() );
		}
		else {
			ShowError( "Failed to load %s database file from '" CL_WHITE "%s" CL_RESET "'.\n", this->type.c_str(), path.c_str() );
			ShowError( "There is likely a syntax error in the file.\n" );
			ShowError( "Error message: %s\n", e.what() );
		}
		aFree(buf);
		return false;
	}

	// Required here already for header error reporting
	this->currentFile = path;

	if (!this->verifyCompatibility(tree)){
		if(strcmpi(MessageLang,"KOR") == 0)
			ShowError("%s은(는) 데이터베이스 파일 '" CL_WHITE "%s" CL_RESET "'과 호환성이 떨어집니다.\n", this->type.c_str(), this->currentFile.c_str());
		else
			ShowError("Failed to verify compatibility with %s database file from '" CL_WHITE "%s" CL_RESET "'.\n", this->type.c_str(), this->currentFile.c_str());
		aFree(buf);
		return false;
	}

	const ryml::NodeRef& header = tree["Header"];

	if( this->nodeExists( header, "Clear" ) ){
		bool clear;

		if( this->asBool( header, "Clear", clear ) && clear ){
			this->clear();
		}
	}

	this->parse( tree );

	this->parseImports( tree );

	aFree(buf);
	return true;
}

void YamlDatabase::loadingFinished(){
	// Does nothing by default, just for hooking
}

void YamlDatabase::parse( const ryml::Tree& tree ){
	uint64 count = 0;

	if( this->nodeExists( tree.rootref(), "Body" ) ){
		const ryml::NodeRef& bodyNode = tree["Body"];
		size_t childNodesCount = bodyNode.num_children();
		const char* fileName = this->currentFile.c_str();
#ifdef DEBUG
		size_t childNodesProgressed = 0;
#endif

		ShowStatus("Loading '" CL_WHITE "%" PRIdPTR CL_RESET "' entries in '" CL_WHITE "%s" CL_RESET "'\n", childNodesCount, fileName);

		for( const ryml::NodeRef &node : bodyNode ){
			count += this->parseBodyNode( node );
#ifdef DETAILED_LOADING_OUTPUT
			if(strcmpi(MessageLang,"KOR") == 0)
				ShowStatus( "[%" PRIdPTR "/%" PRIdPTR "]개의 항목을 '" CL_WHITE "%s" CL_RESET "'에서 불러오는중.." CL_CLL "\r", ++childNodesProgressed, childNodesCount, fileName );
			else
				ShowStatus( "Loading [%" PRIdPTR "/%" PRIdPTR "] entries from '" CL_WHITE "%s" CL_RESET "'" CL_CLL "\r", ++childNodesProgressed, childNodesCount, fileName );
#endif
		}

		if(strcmpi(MessageLang,"KOR") == 0)
			ShowStatus( "'" CL_WHITE "%" PRIu64 CL_RESET "'개의 항목을 '" CL_WHITE "%s" CL_RESET "'에서 불러왔습니다." CL_CLL "\n", count, fileName );
		else
			ShowStatus( "Done reading '" CL_WHITE "%" PRIu64 CL_RESET "' entries in '" CL_WHITE "%s" CL_RESET "'" CL_CLL "\n", count, fileName );
	}
}

void YamlDatabase::parseImports( const ryml::Tree& rootNode ){
	if( this->nodeExists( rootNode.rootref(), "Footer" ) ){
		const ryml::NodeRef& footerNode = rootNode["Footer"];

		if( this->nodeExists( footerNode, "Imports") ){
			const ryml::NodeRef& importsNode = footerNode["Imports"];

			for( const ryml::NodeRef &node : importsNode ){
				std::string importFile;

				if( !this->asString( node, "Path", importFile ) ){
					continue;
				}

				if( this->nodeExists( node, "Mode" ) ){
					std::string mode;

					if( !this->asString( node, "Mode", mode ) ){
						continue;
					}

#ifdef RENEWAL
					std::string compiledMode = "Renewal";

					// RENEWAL mode with RENEWAL_ASPD off, load pre-re ASPD
#ifndef RENEWAL_ASPD
					if (importFile.find("job_aspd.yml") != std::string::npos)
						compiledMode = "Prerenewal";
#endif
#else
					std::string compiledMode = "Prerenewal";
#endif

					if( compiledMode != mode ){
						// Skip this import
						continue;
					}
				}

				if (this->nodeExists(node, "Generator")) {
					bool isGenerator;
					if (!this->asBool(node, "Generator", isGenerator)) {
						continue;
					}
					if (!(shouldLoadGenerator && isGenerator))
						continue; // skip import
				}

				this->load( importFile );
			}
		}
	}
}

template <typename R> bool YamlDatabase::asType( const ryml::NodeRef& node, const std::string& name, R& out ){
	if( this->nodeExists( node, name ) ){
		const ryml::NodeRef& dataNode = node[c4::to_csubstr(name)];

		if (dataNode.val_is_null()) {
			if(strcmpi(MessageLang_Yml,"KOR") == 0)
				this->invalidWarning(node, "Node \"%s\"에 값이 없습니다.\n", name.c_str());
			else
				this->invalidWarning(node, "Node \"%s\" is missing a value.\n", name.c_str());
			return false;
		}

		try{
			dataNode >> out;
		}catch( std::runtime_error const& ){
			if(strcmpi(MessageLang_Yml,"KOR") == 0)
				this->invalidWarning( node, "Node \"%s\"을(를) %s으(로) 해석 할 수 없습니다.\n", name.c_str(), typeid( R ).name() );
			else
				this->invalidWarning( node, "Node \"%s\" cannot be parsed as %s.\n", name.c_str(), typeid( R ).name() );
			return false;
		}

		return true;
	}else{
		if(strcmpi(MessageLang_Yml,"KOR") == 0)
			this->invalidWarning( node, "\"%s\" 노드가 빠졌습니다.\n", name.c_str() );
		else
			this->invalidWarning( node, "Missing node \"%s\".\n", name.c_str() );
		return false;
	}
}

bool YamlDatabase::asBool(const ryml::NodeRef& node, const std::string &name, bool &out) {
	const ryml::NodeRef& targetNode = node[c4::to_csubstr(name)];

	if (targetNode.val_is_null()) {
		if(strcmpi(MessageLang_Yml,"KOR") == 0)
			this->invalidWarning(node, "Node \"%s\"에 값이 빠졌습니다.\n", name.c_str());
		else
			this->invalidWarning(node, "Node \"%s\" is missing a value.\n", name.c_str());
		return false;
	}

	std::string str;

	targetNode >> str;

	util::tolower( str );

	if( str == "true" ){
		out = true;
		return true;
	}else if( str == "false" ){
		out = false;
		return true;
	}else{
		if(strcmpi(MessageLang_Yml,"KOR") == 0)
			this->invalidWarning( targetNode, "알 수 없는 부울 값: \"%s\".\n", str.c_str() );
		else
			this->invalidWarning( targetNode, "Unknown boolean value: \"%s\".\n", str.c_str() );
		return false;
	}
}

bool YamlDatabase::asInt16( const ryml::NodeRef& node, const std::string& name, int16& out ){
	return asType<int16>( node, name, out);
}

bool YamlDatabase::asUInt16(const ryml::NodeRef& node, const std::string& name, uint16& out) {
	return asType<uint16>(node, name, out);
}

bool YamlDatabase::asInt32(const ryml::NodeRef& node, const std::string &name, int32 &out) {
	return asType<int32>(node, name, out);
}

bool YamlDatabase::asUInt32(const ryml::NodeRef& node, const std::string &name, uint32 &out) {
	return asType<uint32>(node, name, out);
}

bool YamlDatabase::asInt64(const ryml::NodeRef& node, const std::string &name, int64 &out) {
	return asType<int64>(node, name, out);
}

bool YamlDatabase::asUInt64(const ryml::NodeRef& node, const std::string &name, uint64 &out) {
	return asType<uint64>(node, name, out);
}

bool YamlDatabase::asFloat(const ryml::NodeRef& node, const std::string &name, float &out) {
	return asType<float>(node, name, out);
}

bool YamlDatabase::asDouble(const ryml::NodeRef& node, const std::string &name, double &out) {
	return asType<double>(node, name, out);
}

bool YamlDatabase::asString(const ryml::NodeRef& node, const std::string &name, std::string &out) {
	return asType<std::string>(node, name, out);
}

bool YamlDatabase::asUInt16Rate( const ryml::NodeRef& node, const std::string& name, uint16& out, uint16 maximum ){
	if( this->asUInt16( node, name, out ) ){
		if( out > maximum ){
			if(strcmpi(MessageLang,"KOR") == 0)
				this->invalidWarning( node[c4::to_csubstr(name)], "\"%s\" 노드의 값 %" PRIu16 "이(가) 최대 값 %" PRIu16 "을(를) 초과합니다.\n", name.c_str(), out, maximum );
			else
				this->invalidWarning( node[c4::to_csubstr(name)], "Node \"%s\" with value %" PRIu16 " exceeds maximum of %" PRIu16 ".\n", name.c_str(), out, maximum );

			return false;
		}else if( out == 0 ){
			if(strcmpi(MessageLang,"KOR") == 0)
				this->invalidWarning( node[c4::to_csubstr(name)], "\"%s\" 노드가 1 이상이어야 합니다.\n", name.c_str() );
			else
				this->invalidWarning( node[c4::to_csubstr(name)], "Node \"%s\" needs to be at least 1.\n", name.c_str() );

			return false;
		}else{
			return true;
		}
	}else{
		return false;
	}
}

bool YamlDatabase::asUInt32Rate( const ryml::NodeRef& node, const std::string& name, uint32& out, uint32 maximum ){
	if( this->asUInt32( node, name, out ) ){
		if( out > maximum ){
			if(strcmpi(MessageLang,"KOR") == 0)
				this->invalidWarning( node[c4::to_csubstr(name)], "노드 \"%s\"의 값 %" PRIu32 "이(가) 최대 값 %" PRIu32 "을(를) 초과합니다.\n", name.c_str(), out, maximum );
			else
				this->invalidWarning( node[c4::to_csubstr(name)], "Node \"%s\" with value %" PRIu32 " exceeds maximum of %" PRIu32 ".\n", name.c_str(), out, maximum );

			return false;
		}else if( out == 0 ){
			if(strcmpi(MessageLang,"KOR") == 0)
				this->invalidWarning( node[c4::to_csubstr(name)], "\"%s\" 노드가 1 이상이어야 합니다.\n", name.c_str() );
			else
				this->invalidWarning( node[c4::to_csubstr(name)], "Node \"%s\" needs to be at least 1.\n", name.c_str() );

			return false;
		}else{
			return true;
		}
	}else{
		return false;
	}
}

int32 YamlDatabase::getLineNumber(const ryml::NodeRef& node) {
	return parser.source().has_str() ? (int32)parser.location(node).line : 0;
}

int32 YamlDatabase::getColumnNumber(const ryml::NodeRef& node) {
	return parser.source().has_str() ? (int32)parser.location(node).col : 0;
}

void YamlDatabase::invalidWarning( const ryml::NodeRef& node, const char* fmt, ... ){
	va_list ap;

	va_start(ap, fmt);

	// Remove any remaining garbage of a previous loading line
	ShowMessage( CL_CLL );
	// Print the actual error
	_vShowMessage( MSG_ERROR, fmt, ap );

	va_end(ap);

	if(strcmpi(MessageLang,"KOR") == 0)
		ShowError( "'" CL_WHITE "%s" CL_RESET "' 파일의 %d째 줄 %d 열에서 발생했습니다.\n", this->currentFile.c_str(), this->getLineNumber(node), this->getColumnNumber(node) );
	else
		ShowError( "Occurred in file '" CL_WHITE "%s" CL_RESET "' on line %d and column %d.\n", this->currentFile.c_str(), this->getLineNumber(node), this->getColumnNumber(node));

#ifdef DEBUG
	std::cout << node;
#endif
}

std::string YamlDatabase::getCurrentFile(){
	return this->currentFile;
}

void YamlDatabase::setGenerator(bool shouldLoad) {
	shouldLoadGenerator = shouldLoad;
}

void on_yaml_error( const char* msg, size_t len, ryml::Location loc, void *user_data ){
	throw std::runtime_error( msg );
}

void do_init_database(){
	ryml::set_callbacks( ryml::Callbacks( nullptr, nullptr, nullptr, on_yaml_error ) );
}
